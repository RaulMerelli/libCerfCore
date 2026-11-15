#include "wave_in_fifo.h"

#include <cerfcore/core/instance_threads.h>

namespace cerfcore {

void WaveInFifo::Start(InstanceThreads& threads, AudioInputs* inputs, const char* log_tag, size_t cap_bytes) {
    fifo_.SetCap(cap_bytes);
    if (inputs == nullptr) return;
    device_ = inputs->CreateInput(log_tag);
    thread_ = threads.Start([this] { Main(); });
}

void WaveInFifo::Stop() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void WaveInFifo::Request(uint32_t sample_rate_hz, uint16_t channels, uint16_t bits) {
    if (!request_.Set(sample_rate_hz, channels, bits)) return;
    std::lock_guard<std::mutex> lk(mtx_);
    cv_.notify_all();
}

void WaveInFifo::Main() {
    bool                         open = false;
    std::unique_lock<std::mutex> lk(mtx_);
    while (!stop_) {
        AudioFormat format;
        if (!request_.TakeChange(format)) {
            cv_.wait(lk, [this] { return stop_ || request_.Pending(); });
            continue;
        }
        lk.unlock();
        if (open) device_->Close();
        fifo_.Clear();
        open = device_->Open(format, [this](const uint8_t* bytes, size_t length) { fifo_.Push(bytes, length); });
        lk.lock();
    }
    lk.unlock();
    if (open) device_->Close();
}

}

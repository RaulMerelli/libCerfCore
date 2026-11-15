#include "wave_out_sink.h"

#include <utility>

namespace cerfcore {

void WaveOutSink::Start(Fatal& fatal, InstanceThreads& threads, std::unique_ptr<AudioOutputDevice> device,
                        StartHandler on_start, CommandHandler on_command, DoneHandler on_done, const char* log_tag) {
    device_  = std::move(device);
    log_tag_ = log_tag;
    callbacks_.Start(fatal, threads, std::move(on_start), std::move(on_command), std::move(on_done),
                     [this] { RetryOpen(); });
}

void WaveOutSink::Stop() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        shutdown_ = true;
        if (open_) {
            device_->Close();
            open_ = false;
        }
    }
    callbacks_.Stop();
}

bool WaveOutSink::EnsureFormat(uint32_t sample_rate_hz, uint16_t channels, uint16_t bits, bool busy) {
    std::unique_lock<std::mutex> lk(mtx_);
    return policy_.EnsureFormat(AudioFormat{sample_rate_hz, channels, bits}, busy, *this, lk);
}

bool WaveOutSink::IsOpen() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return open_;
}

bool WaveOutSink::SamplePosition(uint32_t& samples, uint32_t& generation) const {
    std::lock_guard<std::mutex> lk(mtx_);
    generation      = generation_;
    uint64_t frames = 0;
    if (!open_ || !device_->PlayedFrames(frames)) return false;
    samples = static_cast<uint32_t>(frames);
    return true;
}

void WaveOutSink::Play(uint32_t slot, const uint8_t* bytes, uint32_t length) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (open_) {
        if (device_->Submit(slot, bytes, length)) return;
        LoseDeviceLocked();
    }
    const AudioFormat& req = policy_.Requested();
    callbacks_.QueueSilent(slot, length, req.rate_hz, req.channels, req.bits, log_tag_);
    if (policy_.ShouldPostRetry()) callbacks_.PostRetryOpen();
}

void WaveOutSink::Reset() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (open_) device_->Drop();
    callbacks_.CompleteSilent();
}

bool WaveOutSink::OpenDeviceLocked(std::unique_lock<std::mutex>& lk) {
    policy_.NoteAttempt();
    const AudioFormat format = policy_.Requested();
    lk.unlock();
    const bool opened = device_->Open(format, [this](uint32_t block) { callbacks_.PostDone(block); });
    lk.lock();
    if (!opened) return false;
    if (shutdown_) {
        device_->Close();
        return false;
    }
    open_ = true;
    policy_.NoteOpened();
    ++generation_;
    return true;
}

void WaveOutSink::CloseDeviceLocked(std::unique_lock<std::mutex>&) {
    device_->Close();
    open_ = false;
}

void WaveOutSink::LoseDeviceLocked() {
    device_->Close();
    open_ = false;
    policy_.NoteAttempt();
}

void WaveOutSink::RetryOpen() {
    std::unique_lock<std::mutex> lk(mtx_);
    if (policy_.BeginRetry(open_) && !shutdown_) OpenDeviceLocked(lk);
}

}

#pragma once

#include "wave_out_callback_loop.h"
#include "wave_out_reopen_policy.h"

#include <cerfcore/host/audio_outputs.h>

#include <cstdint>
#include <memory>
#include <mutex>

namespace cerfcore {

class Fatal;
class InstanceThreads;

class WaveOutSink final : private WaveOutDevice {
public:
    static constexpr uint32_t kSlots  = 8;
    static constexpr uint32_t kNoSlot = kSlots;

    using StartHandler   = WaveOutCallbackLoop::StartHandler;
    using CommandHandler = WaveOutCallbackLoop::CommandHandler;
    using DoneHandler    = WaveOutCallbackLoop::DoneHandler;

    WaveOutSink() = default;
    ~WaveOutSink() { Stop(); }
    WaveOutSink(const WaveOutSink&)            = delete;
    WaveOutSink& operator=(const WaveOutSink&) = delete;

    void Start(Fatal& fatal, InstanceThreads& threads, std::unique_ptr<AudioOutputDevice> device,
               StartHandler on_start, CommandHandler on_command, DoneHandler on_done, const char* log_tag);
    void Stop();
    void PostCommand(uint32_t command) { callbacks_.PostCommand(command); }

    bool EnsureFormat(uint32_t sample_rate_hz, uint16_t channels, uint16_t bits, bool busy);
    bool IsOpen() const;
    bool SamplePosition(uint32_t& samples, uint32_t& generation) const;

    void Play(uint32_t slot, const uint8_t* bytes, uint32_t length);
    void Reset();

private:
    bool DeviceOpenLocked() const override { return open_; }
    void CloseDeviceLocked(std::unique_lock<std::mutex>& lk) override;
    bool OpenDeviceLocked(std::unique_lock<std::mutex>& lk) override;
    void LoseDeviceLocked();
    void RetryOpen();

    mutable std::mutex                 mtx_;
    WaveOutReopenPolicy                policy_;
    WaveOutCallbackLoop                callbacks_;
    std::unique_ptr<AudioOutputDevice> device_;
    uint32_t                           generation_ = 0;
    const char*                        log_tag_    = "WaveOutSink";
    bool                               open_       = false;
    bool                               shutdown_   = false;
};

}

#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_pointer_feature.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace cerfcore {

class KeyboardRouter;
class TouchInput;

class StylusAltTap : public Service, public MachinePointerFeature {
public:
    using Service::Service;

    void OnReady() override;
    void OnShutdown() override;

    std::string ObjectId() const override { return "alt_tap"; }
    static constexpr char kObjectName[] = "Alt+Tap";
    std::string ObjectName() const override { return kObjectName; }
    bool        Available() const override { return Armable(); }
    bool        Enabled() const override { return enabled_.load(std::memory_order_acquire); }
    void        SetEnabled(bool on) override {
        enabled_.store(on, std::memory_order_release);
        Refresh();
    }
    void        SaveObjectState(StateWriter& w) const override;
    void        RestoreObjectState(StateReader& r) override;

    bool Supported() const;
    bool Armable() const;
    bool Begin(int guest_x, int guest_y);
    bool Running() const;
    void Cancel();

private:
    enum class Phase : uint8_t { Idle, AltLead, PenHold, AltTrail };
    using Clock = std::chrono::steady_clock;

    struct Unwind {
        Phase saved = Phase::Idle;
        int   x     = 0;
        int   y     = 0;
    };

    void Run();
    void StepLocked();
    void ArmLocked(uint32_t ms, Phase next);
    void UnwindHeldLocked();
    void ApplyUnwindLocked(const Unwind& unwind);

    KeyboardRouter*                  keyboard_ = nullptr;
    TouchInput*                      touch_    = nullptr;
    mutable std::mutex               mtx_;
    std::condition_variable          wake_;
    std::thread                      worker_;
    bool                             stop_  = false;
    Phase                            phase_ = Phase::Idle;
    std::optional<Clock::time_point> deadline_;
    std::optional<Unwind>            unwind_;
    int                              tap_x_ = 0;
    int                              tap_y_ = 0;
    std::atomic<bool>                enabled_{true};
};

}

#include "stylus_alt_tap.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/host/keyboard_input.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/host/keyboard_router.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/host/pointer_router.h>
#include <cerfcore/host/touch_input.h>
#include <cerfcore/host/virtual_keys.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

REGISTER_SERVICE(StylusAltTap);

namespace {

constexpr uint32_t kAltLeadMs  = 500;
constexpr uint32_t kPenHoldMs  = 150;
constexpr uint32_t kAltTrailMs = 500;

}

void StylusAltTap::OnReady() {
    if (!Supported()) return;
    keyboard_ = &emu_.Get<KeyboardRouter>();
    touch_    = &emu_.Get<TouchInput>();
    auto& tree = emu_.Get<MachineObjectTree>();
    tree.Add(this, &emu_.Get<PointerRouter>());
    worker_ = emu_.Get<InstanceThreads>().Start([this] { Run(); });
    const MachineObject* keyboard = keyboard_;
    tree.Subscribe([this, keyboard](const MachineEvent& event) {
        if (event.kind == MachineEventKind::kChanged && event.object == keyboard) Refresh();
    });
}

void StylusAltTap::OnShutdown() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

bool StylusAltTap::Supported() const {
    if (!emu_.TryGet<TouchInput>()) return false;
    if (emu_.Get<DeviceConfig>().guest_additions) return true;
    auto*    map = emu_.TryGet<KeyboardMap>();
    uint32_t code;
    return map && map->BaseDeviceCode(kVkMenu, code);
}

bool StylusAltTap::Armable() const {
    if (!worker_.joinable()) return false;
    KeyboardInput* kbd = keyboard_->Active();
    return kbd && kbd->CanDeliverVk(kVkMenu);
}

bool StylusAltTap::Running() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return phase_ != Phase::Idle;
}

void StylusAltTap::ArmLocked(uint32_t ms, Phase next) {
    phase_    = next;
    deadline_ = Clock::now() + std::chrono::milliseconds(ms);
    wake_.notify_all();
}

bool StylusAltTap::Begin(int guest_x, int guest_y) {
    if (!Enabled() || !Armable()) return false;
    std::lock_guard<std::mutex> lk(mtx_);
    if (phase_ != Phase::Idle) return true;
    tap_x_ = guest_x;
    tap_y_ = guest_y;
    keyboard_->OnHostKey(kVkMenu, false);
    ArmLocked(kAltLeadMs, Phase::AltLead);
    return true;
}

void StylusAltTap::Cancel() {
    std::lock_guard<std::mutex> lk(mtx_);
    UnwindHeldLocked();
}

void StylusAltTap::UnwindHeldLocked() {
    if (phase_ == Phase::Idle) return;
    const bool pen_down = phase_ == Phase::PenHold;
    phase_              = Phase::Idle;
    deadline_.reset();
    if (pen_down) touch_->OnPenUp(tap_x_, tap_y_);
    keyboard_->OnHostKey(kVkMenu, true);
}

void StylusAltTap::StepLocked() {
    switch (phase_) {
        case Phase::AltLead:
            touch_->OnPenDown(tap_x_, tap_y_);
            ArmLocked(kPenHoldMs, Phase::PenHold);
            return;
        case Phase::PenHold:
            touch_->OnPenUp(tap_x_, tap_y_);
            ArmLocked(kAltTrailMs, Phase::AltTrail);
            return;
        case Phase::AltTrail:
            phase_ = Phase::Idle;
            keyboard_->OnHostKey(kVkMenu, true);
            return;
        case Phase::Idle:
            return;
    }
}

void StylusAltTap::ApplyUnwindLocked(const Unwind& unwind) {
    phase_ = Phase::Idle;
    deadline_.reset();
    if (unwind.saved == Phase::Idle) return;
    if (unwind.saved == Phase::PenHold) touch_->OnPenUp(unwind.x, unwind.y);
    keyboard_->OnHostKey(kVkMenu, true);
}

void StylusAltTap::Run() {
    std::unique_lock<std::mutex> lk(mtx_);
    for (;;) {
        wake_.wait(lk, [this] { return stop_ || unwind_ || deadline_; });
        if (stop_) return;
        if (unwind_) {
            const Unwind unwind = *unwind_;
            unwind_.reset();
            ApplyUnwindLocked(unwind);
            continue;
        }
        const Clock::time_point due = *deadline_;
        if (wake_.wait_until(lk, due, [this, due] { return stop_ || unwind_ || !deadline_ || *deadline_ != due; }))
            continue;
        deadline_.reset();
        StepLocked();
    }
}

void StylusAltTap::SaveObjectState(StateWriter& w) const {
    std::lock_guard<std::mutex> lk(mtx_);
    w.Write<uint8_t>("alt_tap_enabled", Enabled() ? 1u : 0u);
    w.Write<uint8_t>("phase", static_cast<uint8_t>(phase_));
    w.Write<int32_t>("tap_x", tap_x_);
    w.Write<int32_t>("tap_y", tap_y_);
}

void StylusAltTap::RestoreObjectState(StateReader& r) {
    uint8_t enabled = 1;
    uint8_t raw     = 0;
    int32_t x = 0, y = 0;
    r.Read("alt_tap_enabled", enabled);
    r.Read("phase", raw);
    r.Read("tap_x", x);
    r.Read("tap_y", y);
    SetEnabled(enabled != 0);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const Phase saved = static_cast<Phase>(raw);
        if (saved == Phase::Idle && phase_ == Phase::Idle) return;
        unwind_ = Unwind{saved, x, y};
        deadline_.reset();
    }
    wake_.notify_all();
}

}

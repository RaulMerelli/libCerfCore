#include <cerfcore/host/pointer_router.h>

#include "calibration_run.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/host/pointer_input.h>
#include <cerfcore/host/pointer_source.h>
#include "pointer_stylus_simulation.h"
#include <cerfcore/host/relative_mouse_input.h>
#include "stylus_alt_tap.h"
#include <cerfcore/host/touch_input.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

REGISTER_SERVICE(PointerRouter);

namespace {

using Kind = HostPointerEvent::Kind;

}

void PointerRouter::OnReady() {
    if (auto* p = emu_.TryGet<PointerInput>())       Register(p);
    if (auto* r = emu_.TryGet<RelativeMouseInput>()) Register(r);
    if (auto* t = emu_.TryGet<TouchInput>())         Register(t);
    emu_.Get<MachineObjectTree>().Add(this);
}

void PointerRouter::SaveObjectState(StateWriter& w) const {
    SaveSelection(w);
    if (const CalibrationRun* run = emu_.TryGet<CalibrationRun>()) run->SaveState(w);
}

void PointerRouter::RestoreObjectState(StateReader& r) {
    RestoreSelection(r);
    if (auto* run = emu_.TryGet<CalibrationRun>()) run->RestoreState(r);
}

uint32_t PointerRouter::RelativeButtonsOf(uint32_t held) {
    uint32_t mask = 0;
    if (held & kHeldLeft) mask |= kRelMouseLeft;
    if (held & kHeldRight) mask |= kRelMouseRight;
    return mask;
}

uint32_t PointerRouter::PointerButtonsOf(uint32_t held) {
    uint32_t mask = 0;
    if (held & kHeldLeft) mask |= kPointerLeft;
    if (held & kHeldRight) mask |= kPointerRight;
    if (held & kHeldMiddle) mask |= kPointerMiddle;
    return mask;
}

bool PointerRouter::Route(const HostPointerEvent& event) {
    if (pen_down_) return RouteStylus(event);
    if (pointer_buttons_ != 0) return RoutePointer(event);
    PointerKind kind = PointerKind::Absolute;
    if (PointerSource* active = Active()) kind = active->Kind();
    if (kind == PointerKind::Absolute) return RoutePointer(event);
    if (kind == PointerKind::Stylus) return RouteStylus(event);
    return false;
}

void PointerRouter::ReleaseLost(const HostPointerEvent& event) {
    if (event.kind == Kind::kCaptureLost) return;
    uint32_t held = event.held;
    if (event.kind == Kind::kDown) held &= ~HostButtonBit(event.button);
    if (event.kind == Kind::kUp) held |= HostButtonBit(event.button);
    if (pen_down_ && !(held & kHeldLeft)) LiftPen(event);
    const uint32_t pointer_held = PointerButtonsOf(held);
    if (pointer_buttons_ & ~pointer_held) {
        pointer_buttons_ &= pointer_held;
        emu_.Get<PointerInput>().OnMove(event.x, event.y, pointer_buttons_);
    }
    const uint32_t relative_held = RelativeButtonsOf(held);
    if (relative_buttons_ & ~relative_held) {
        relative_buttons_ &= relative_held;
        emu_.Get<RelativeMouseInput>().OnRelativeMove(0, 0, relative_buttons_);
    }
}

void PointerRouter::OnRelativeMove(int dx, int dy, uint32_t buttons) {
    emu_.Get<RelativeMouseInput>().OnRelativeMove(dx, dy, buttons);
    relative_buttons_ = buttons;
}

void PointerRouter::ReleasePen() {
    emu_.Get<StylusAltTap>().Cancel();
    if (!pen_down_) return;
    pen_down_ = false;
    if (auto* touch = emu_.TryGet<TouchInput>()) touch->OnCaptureLost();
}

void PointerRouter::ReleasePointerButtons() {
    if (pointer_buttons_ == 0) return;
    pointer_buttons_ = 0;
    emu_.Get<PointerInput>().OnCaptureLost();
}

void PointerRouter::ReleaseRelative() {
    if (relative_buttons_ == 0) return;
    relative_buttons_ = 0;
    emu_.Get<RelativeMouseInput>().OnRelativeMove(0, 0, 0);
}

void PointerRouter::LiftPen(const HostPointerEvent& event) {
    pen_down_ = false;
    if (auto* touch = emu_.TryGet<TouchInput>()) touch->OnPenUp(event.x, event.y);
}

bool PointerRouter::RoutePointer(const HostPointerEvent& event) {
    auto* pointer = emu_.TryGet<PointerInput>();
    if (!pointer) return false;
    if (event.kind == Kind::kWheel) {
        pointer->OnWheel(event.x, event.y, event.wheel);
        return true;
    }
    if (event.kind == Kind::kCaptureLost) {
        ReleasePointerButtons();
        return true;
    }
    const bool down = event.kind == Kind::kDown && event.button != HostButton::kNone;
    const bool up   = event.kind == Kind::kUp && event.button != HostButton::kNone;
    if (!down && !up && event.kind != Kind::kMove) return false;
    const uint32_t mask   = PointerButtonsOf(event.held);
    const bool     held   = (mask & (kPointerLeft | kPointerRight)) != 0;
    const bool     report = down || up || held || !emu_.Get<PointerStylusSimulation>().Enabled();
    if (report) {
        pointer->OnMove(event.x, event.y, mask);
        pointer_buttons_ = mask;
    }
    return true;
}

bool PointerRouter::RouteStylus(const HostPointerEvent& event) {
    auto*         touch   = emu_.TryGet<TouchInput>();
    StylusAltTap& alt_tap = emu_.Get<StylusAltTap>();
    if (event.kind == Kind::kDown && event.button == HostButton::kRight) {
        if (!pen_down_ && event.inside) alt_tap.Begin(event.x, event.y);
        return true;
    }
    if (event.kind == Kind::kUp && event.button == HostButton::kRight) return true;
    if (event.kind == Kind::kDown && event.button == HostButton::kLeft) {
        if (alt_tap.Running() || !event.inside) return true;
        pen_down_ = true;
        if (touch) touch->OnPenDown(event.x, event.y);
        return true;
    }
    if (event.kind == Kind::kMove) {
        if (pen_down_ && touch) touch->OnPenMove(event.x, event.y);
        return true;
    }
    if (event.kind == Kind::kUp && event.button == HostButton::kLeft) {
        if (pen_down_) LiftPen(event);
        return true;
    }
    if (event.kind == Kind::kCaptureLost) {
        if (pen_down_) {
            pen_down_ = false;
            if (touch) touch->OnCaptureLost();
        }
        return true;
    }
    return false;
}

}

#include "vrc5477_timers.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../cpu/vr5500/vr5500_board_clock.h"
#include "../../cpu/vr5500/vr5500_id.h"
#include "vrc5477_intc.h"

namespace cerfcore {

namespace {

constexpr uint32_t kBlockFirst  = 0x1C0u;
constexpr uint32_t kBlockEnd    = 0x200u;
constexpr uint32_t kRefreshCtrl = 0x1C0u;
constexpr uint32_t kSpt0Ctrl    = 0x1D0u;
constexpr uint32_t kSpt1Ctrl    = 0x1E0u;
constexpr uint32_t kUnitMask    = ~0xFu;
constexpr uint32_t kRegMask     = 0xCu;
constexpr uint32_t kOffCtrlLo   = 0x0u;
constexpr uint32_t kOffCtrlHi   = 0x4u;
constexpr uint32_t kOffCount    = 0x8u;

constexpr uint32_t kCtrlEnable       = 1u << 0;
constexpr uint32_t kCtrlPrescale     = 1u << 1;
constexpr uint32_t kCtrlSourceSpt0   = 1u << 2;
constexpr uint32_t kSpt1CascadedCtrl = kCtrlEnable | kCtrlPrescale | kCtrlSourceSpt0;

constexpr uint32_t kSpt0ValueLimit = 1u << 27;

constexpr uint32_t kSpt0Source = 21u;
constexpr uint32_t kSpt1Source = 6u;

}

REGISTER_SERVICE(Vrc5477Timers);

bool Vrc5477Timers::ShouldRegister() {
    return emu_.Get<BoardContext>().GetSocId() == SocId::Vr5500;
}

void Vrc5477Timers::OnReady() {
    clock_ = &emu_.Get<GuestCycleClock>();
    intc_  = &emu_.Get<Vrc5477Intc>();
    const Vr5500BoardClock& board = emu_.Get<Vr5500BoardClock>();
    sys_.Attach(board.PClockHz() * 2u, board.DivMode() + 4u);
    for (Spt& s : spt_) s.event = clock_->Add([this] { OnEvent(); });
    ResetState();
    clock_->RegisterRateListener([this] { OnRateChange(); });
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { ResetState(); });
}

bool Vrc5477Timers::Owns(uint32_t off) { return off >= kBlockFirst && off < kBlockEnd; }

bool Vrc5477Timers::Cascaded() const { return (spt1_control_ & kCtrlPrescale) != 0u; }

uint64_t Vrc5477Timers::Spt1Tick(uint64_t sys_tick) const {
    return Cascaded() ? spt0_terminals_ : sys_tick;
}

uint64_t Vrc5477Timers::Sync() {
    const uint64_t now       = sys_.Now();
    ReloadDownCounter& spt0  = spt_[0].counter;
    ReloadDownCounter& spt1  = spt_[1].counter;
    const uint64_t terminals = spt0.FlagsThrough(now);
    spt0.Advance(now);
    if (terminals != 0u) {
        intc_->LatchSource(kSpt0Source);
        spt0_terminals_ += terminals;
    }
    const uint64_t tick1 = Spt1Tick(now);
    if (spt1.FlagsThrough(tick1) != 0u) intc_->LatchSource(kSpt1Source);
    spt1.Advance(tick1);
    return now;
}

void Vrc5477Timers::Arm() {
    Spt& spt0 = spt_[0];
    Spt& spt1 = spt_[1];
    if (spt0.counter.Running()) sys_.ArmAt(spt0.event, spt0.counter.NextFlag());
    else                        clock_->Disarm(spt0.event);
    if (spt1.counter.Running() && !Cascaded()) sys_.ArmAt(spt1.event, spt1.counter.NextFlag());
    else                                       clock_->Disarm(spt1.event);
}

void Vrc5477Timers::OnEvent() {
    Sync();
    Arm();
}

void Vrc5477Timers::OnRateChange() {
    Sync();
    sys_.Rescale();
    Arm();
}

void Vrc5477Timers::ResetState() {
    for (Spt& s : spt_) {
        s.counter.PowerOnReset(0u);
        s.reload_written = false;
    }
    spt1_control_    = 0u;
    spt0_terminals_  = 0u;
    refresh_reload_  = 0u;
    refresh_control_ = 0u;
    Arm();
}

void Vrc5477Timers::Reject(const char* what, uint32_t off, uint32_t value) {
    emu_.Get<Fatal>().Die("VRC5477 timers: %s off=0x%03X val=0x%08X", what, off, value);
}

void Vrc5477Timers::StartFromReload(Spt& spt, uint64_t tick) {
    if (spt.counter.Constant() == 0u) {
        emu_.Get<Fatal>().Die("VRC5477 timers: enable with reload 0 (SPT%u)",
                              static_cast<unsigned>(&spt - spt_));
    }
    spt.counter.WriteCount(tick, spt.counter.Constant());
    spt.counter.Start(tick);
}

uint32_t Vrc5477Timers::ReadReg(uint32_t off) {
    const uint32_t unit = off & kUnitMask;
    if ((off & kRegMask) != kOffCount || (unit != kSpt0Ctrl && unit != kSpt1Ctrl)) {
        Reject("read", off, 0u);
    }
    const uint64_t now = Sync();
    Spt&           spt = unit == kSpt0Ctrl ? spt_[0] : spt_[1];
    if (!spt.counter.Running()) Reject("counter read while stopped", off, 0u);
    return spt.counter.Count(unit == kSpt0Ctrl ? now : Spt1Tick(now));
}

void Vrc5477Timers::WriteReg(uint32_t off, uint32_t value) {
    const uint32_t unit = off & kUnitMask;
    if (unit == kRefreshCtrl) {
        WriteRefresh(off, value);
    } else if (unit == kSpt0Ctrl) {
        WriteSpt0(off, value);
    } else if (unit == kSpt1Ctrl) {
        WriteSpt1(off, value);
    } else {
        Reject("watchdog timer write", off, value);
    }
}

void Vrc5477Timers::WriteRefresh(uint32_t off, uint32_t value) {
    const uint32_t reg = off & kRegMask;
    if (reg == kOffCtrlLo) {
        refresh_reload_ = value;
    } else if (reg == kOffCtrlHi) {
        refresh_control_ = value;
    } else {
        Reject("refresh counter write", off, value);
    }
}

void Vrc5477Timers::WriteSpt0(uint32_t off, uint32_t value) {
    const uint64_t     now     = Sync();
    Spt&               spt     = spt_[0];
    ReloadDownCounter& counter = spt.counter;
    const uint32_t     reg     = off & kRegMask;
    if (counter.Running()) Reject("SPT0 write while counting", off, value);
    if (reg == kOffCtrlLo) {
        if (value >= kSpt0ValueLimit) Reject("SPT0 reload write", off, value);
        counter.WriteConstant(now, value);
        spt.reload_written = true;
    } else if (reg == kOffCtrlHi) {
        if (value != kCtrlEnable || !spt.reload_written) Reject("SPT0 control write", off, value);
        StartFromReload(spt, now);
    } else if (reg == kOffCount) {
        if (value >= kSpt0ValueLimit) Reject("SPT0 counter write", off, value);
        counter.WriteCount(now, value);
    } else {
        Reject("SPT0 counter-high write", off, value);
    }
    Arm();
}

void Vrc5477Timers::WriteSpt1(uint32_t off, uint32_t value) {
    const uint64_t     now     = Sync();
    Spt&               spt     = spt_[1];
    ReloadDownCounter& counter = spt.counter;
    const uint32_t     reg     = off & kRegMask;
    const uint64_t     tick    = Spt1Tick(now);
    if (reg == kOffCtrlLo) {
        if (counter.Running()) Reject("SPT1 reload write while counting", off, value);
        counter.WriteConstant(tick, value);
        spt.reload_written = true;
    } else if (reg == kOffCtrlHi) {
        if (counter.Running()) {
            if (value != 0u) Reject("SPT1 control write while counting", off, value);
            counter.Stop(tick);
            spt1_control_ = 0u;
        } else if ((value == kCtrlEnable || value == kSpt1CascadedCtrl) && spt.reload_written) {
            spt1_control_ = value;
            StartFromReload(spt, Spt1Tick(now));
        } else {
            Reject("SPT1 control write", off, value);
        }
    } else if (reg == kOffCount) {
        if (counter.Running() && value == 0u) Reject("SPT1 counter write while counting", off, value);
        counter.WriteCount(tick, value);
    } else {
        Reject("SPT1 counter-high write", off, value);
    }
    Arm();
}

void Vrc5477Timers::SaveState(StateWriter& w) {
    const uint64_t now = Sync();
    sys_.Save(w, "vrc5477_sys_");
    w.Write("vrc5477_spt1_control", spt1_control_);
    w.Write("vrc5477_spt0_terminals", spt0_terminals_);
    w.Write("vrc5477_refresh_reload", refresh_reload_);
    w.Write("vrc5477_refresh_control", refresh_control_);
    w.Write("vrc5477_spt0_reload_written", spt_[0].reload_written);
    w.Write("vrc5477_spt1_reload_written", spt_[1].reload_written);
    spt_[0].counter.Save(w, "vrc5477_spt0_", now);
    spt_[1].counter.Save(w, "vrc5477_spt1_", Spt1Tick(now));
}

void Vrc5477Timers::RestoreState(StateReader& r) {
    sys_.Restore(r, "vrc5477_sys_");
    r.Read("vrc5477_spt1_control", spt1_control_);
    r.Read("vrc5477_spt0_terminals", spt0_terminals_);
    r.Read("vrc5477_refresh_reload", refresh_reload_);
    r.Read("vrc5477_refresh_control", refresh_control_);
    r.Read("vrc5477_spt0_reload_written", spt_[0].reload_written);
    r.Read("vrc5477_spt1_reload_written", spt_[1].reload_written);
    spt_[0].counter.Restore(r, "vrc5477_spt0_");
    spt_[1].counter.Restore(r, "vrc5477_spt1_");
    const uint64_t now = sys_.Now();
    spt_[0].counter.Rebase(now);
    spt_[1].counter.Rebase(Spt1Tick(now));
    Arm();
}

}

#pragma once

#include <cstdint>
#include <string_view>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include <cerfengine/sh/sh_cpu.h>
#include "../../execution/engine_bridge.h"
#include "../../execution/guest_cycle_clock.h"
#include "../cycle_anchored_counter.h"
#include "../reg_window_impl.h"
#include "sh77xx_frqcr.h"
#include "sh77xx_peripheral_clock_ticks.h"
#include "sh77xx_wdt.h"

namespace cerfcore {

namespace sh77xx_wdt {

using cerf_reg_window_detail::OtherReset;
using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegSpec;
using cerf_reg_window_detail::RegWindowBase;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

inline constexpr uint32_t kBase = 0x1FFFFF84u;

inline constexpr uint32_t kWtcnt = 0u;
inline constexpr uint32_t kWtcsr = 1u;

inline constexpr uint16_t kWtcntKey   = 0x5Au;
inline constexpr uint16_t kWtcsrKey   = 0xA5u;
inline constexpr uint16_t kData       = 0x00FFu;
inline constexpr uint16_t kWtcsrTme   = 0x0080u;
inline constexpr uint16_t kWtcsrWt    = 0x0040u;
inline constexpr uint16_t kWtcsrRsts  = 0x0020u;
inline constexpr uint16_t kWtcsrWovf  = 0x0010u;
inline constexpr uint16_t kWtcsrCks   = 0x0007u;
inline constexpr uint16_t kWtcsrClock = kWtcsrWt | kWtcsrCks;

inline constexpr uint32_t kCksShift[8]         = {0u, 2u, 4u, 5u, 6u, 8u, 10u, 12u};
inline constexpr uint64_t kCounterSpan         = 0x100u;
inline constexpr uint64_t kStandbyReentryCount = 0x80u;

enum class Count : uint8_t { kNone, kStandbyExit, kWatchdog };

constexpr RegSpec Register() {
    return RegSpec{ReadKind::kFatal, WriteKind::kStored, kData, 0u, 0u, OtherReset::kRetain};
}

constexpr RegWindowModel WindowOf() {
    RegWindowModel w{kBase, 4u, 2u, false, {}};
    w.reg[kWtcnt] = Register();
    w.reg[kWtcsr] = Register();
    return w;
}

inline constexpr RegWindowModel kWindow = WindowOf();

template <const std::string_view& Soc>
class Sh77xxWdtBase : public RegWindowBase<Soc, kWindow, Sh77xxWdt> {
    using Base = RegWindowBase<Soc, kWindow, Sh77xxWdt>;

public:
    using Base::Base;

    void OnReady() override {
        Base::OnReady();
        clock_ = &this->emu_.template Get<GuestCycleClock>();
        cpu_   = &this->emu_.template Get<EngineBridge>().Engine().template Get<cerfengine::ShCpu>();
        frqcr_ = this->emu_.template TryGet<Sh77xxFrqcr>();
        event_ = clock_->Add([this] { OnCountEnd(); });
        if (frqcr_ != nullptr) {
            pclk_.Attach([this] { Retime(); });
            frqcr_->RegisterChangeListener([this](uint16_t from, uint16_t to) { OnFrequencyChange(from, to); });
        }
        this->emu_.template Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) { OnReset(kind); });
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t reg = (addr - kBase) / 2u;
        const uint16_t key = reg == kWtcnt ? kWtcntKey : kWtcsrKey;
        if ((value >> 8) != key) this->HaltUnsupportedAccess("WriteHalf without the register key", addr, value);
        if (count_ == Count::kStandbyExit) {
            this->emu_.template Get<Fatal>().Die("Sh77xxWdt: write of 0x%04X at 0x%08X while the WDT counts after a standby exit",
                                                 value, addr);
        }
        const uint16_t data = static_cast<uint16_t>(value & kData);
        if (count_ == Count::kWatchdog) {
            WriteWhileCounting(addr, reg, data);
            return;
        }
        Base::WriteHalf(addr, data);
        if (reg == kWtcsr && (data & kWtcsrTme) != 0u) StartWatchdog(this->StoredReg(kWtcnt));
    }

    void CountAfterStandby() override {
        if (count_ == Count::kWatchdog) {
            this->emu_.template Get<Fatal>().Die("Sh77xxWdt: a standby exit while WTCSR 0x%02X has TME set",
                                                 this->StoredReg(kWtcsr));
        }
        StartCount(Count::kStandbyExit, 0u);
    }

    void StopStandbyCount() override {
        if (count_ == Count::kStandbyExit) Disarm();
    }

    void SaveState(StateWriter& w) override {
        Base::SaveState(w);
        if (frqcr_ != nullptr) pclk_.Save(w);
        w.Write("count", static_cast<uint8_t>(count_));
        w.Write("live_count", count_ == Count::kNone ? uint16_t{0u} : CurrentCount());
    }

    void RestoreState(StateReader& r) override {
        Base::RestoreState(r);
        if (frqcr_ != nullptr) pclk_.Restore(r);
        uint8_t count = 0u;
        r.Read("count", count);
        count_ = static_cast<Count>(count);
        r.Read("live_count", restored_count_);
    }

    void PostRestore() override {
        if (frqcr_ != nullptr) pclk_.PostRestore();
        if (count_ == Count::kNone) {
            clock_->Disarm(event_);
            return;
        }
        counter_.Anchor(DividedTicks(), restored_count_);
        Arm();
    }

private:
    void WriteWhileCounting(uint32_t addr, uint32_t reg, uint16_t data) {
        if (reg == kWtcnt) {
            Base::WriteHalf(addr, data);
            StartWatchdog(data);
            return;
        }
        if ((data & kWtcsrTme) == 0u) {
            this->SetStoredReg(kWtcnt, CurrentCount());
            Disarm();
            Base::WriteHalf(addr, data);
            return;
        }
        const uint16_t csr = this->StoredReg(kWtcsr);
        if (((data ^ csr) & kWtcsrClock) != 0u) {
            this->emu_.template Get<Fatal>().Die(
                "Sh77xxWdt: WTCSR write of 0x%02X changes WT/IT or CKS while the WDT counts (WTCSR 0x%02X)", data, csr);
        }
        Base::WriteHalf(addr, data);
    }

    void StartWatchdog(uint16_t value) {
        const uint16_t csr = this->StoredReg(kWtcsr);
        if ((csr & kWtcsrWt) == 0u) {
            this->emu_.template Get<Fatal>().Die("Sh77xxWdt: WTCSR 0x%02X starts the interval timer mode", csr);
        }
        StartCount(Count::kWatchdog, value);
    }

    void StartCount(Count mode, uint16_t value) {
        if (frqcr_ == nullptr) {
            this->emu_.template Get<Fatal>().Die("Sh77xxWdt: counting on %.*s needs FRQCR, and no FRQCR model exists for it",
                                                 static_cast<int>(Soc.size()), Soc.data());
        }
        count_ = mode;
        counter_.Anchor(DividedTicks(), value);
        Arm();
    }

    uint32_t Shift() const { return kCksShift[this->StoredReg(kWtcsr) & kWtcsrCks]; }

    uint64_t DividedTicks() const { return pclk_.DividedTicks(Shift()); }

    uint16_t CurrentCount() const { return static_cast<uint16_t>(counter_.CountAt(DividedTicks()) & kData); }

    uint64_t EndTick() const {
        const uint64_t end = count_ == Count::kWatchdog ? kCounterSpan : kStandbyReentryCount;
        return counter_.CycleOfTick(end - counter_.AnchorCount());
    }

    void Arm() { clock_->Arm(event_, pclk_.CycleOfDividedTick(EndTick(), Shift())); }

    void Disarm() {
        count_ = Count::kNone;
        clock_->Disarm(event_);
    }

    void Retime() {
        if (count_ != Count::kNone) Arm();
    }

    void OnFrequencyChange(uint16_t from, uint16_t to) {
        if (count_ == Count::kNone || from == to) return;
        this->emu_.template Get<Fatal>().Die("Sh77xxWdt: FRQCR changed from 0x%04X to 0x%04X while the WDT counts", from,
                                             to);
    }

    void OnReset(ResetLineKind kind) {
        if (count_ == Count::kStandbyExit || kind == ResetLineKind::Rtc) Disarm();
    }

    void OnCountEnd() {
        const uint16_t csr = this->StoredReg(kWtcsr);
        if (count_ == Count::kStandbyExit) {
            this->emu_.template Get<Fatal>().Die(
                "Sh77xxWdt: WTCNT reached H'80 after a standby exit with STBCR.STBY set (WTCSR CKS %u)", csr & kWtcsrCks);
        }
        if ((csr & kWtcsrRsts) == 0u) {
            this->emu_.template Get<Fatal>().Die(
                "Sh77xxWdt: WTCNT overflow in watchdog timer mode selects a power-on reset (WTCSR 0x%02X)", csr);
        }
        this->SetStoredReg(kWtcsr, static_cast<uint16_t>(csr | kWtcsrWovf));
        counter_.Anchor(EndTick(), 0u);
        Arm();
        cpu_->SetPendingResetKind(cerfengine::ShResetKind::kManual);
        this->emu_.template Get<GuestCpuReset>().ResetWithLatchedCause(ResetLineKind::Watchdog);
    }

    GuestCycleClock*           clock_ = nullptr;
    GuestCycleClock::Event*    event_ = nullptr;
    cerfengine::ShCpu*         cpu_   = nullptr;
    Sh77xxFrqcr*               frqcr_ = nullptr;
    Sh77xxPeripheralClockTicks pclk_{this->emu_};
    TickAnchoredCounter        counter_;
    uint16_t                   restored_count_ = 0u;
    Count                      count_          = Count::kNone;
};

}

}

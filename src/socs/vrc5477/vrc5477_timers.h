#pragma once

#include <cerfcore/core/service.h>
#include "../../execution/guest_cycle_clock.h"
#include "../oscillator_ticks.h"
#include "../reload_down_counter.h"

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;
class Vrc5477Intc;

class Vrc5477Timers : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    static bool Owns(uint32_t off);
    uint32_t    ReadReg(uint32_t off);
    void        WriteReg(uint32_t off, uint32_t value);

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

private:
    struct Spt {
        ReloadDownCounter       counter{ReloadDownCounter::FlagPoint::kZero};
        GuestCycleClock::Event* event           = nullptr;
        bool                    reload_written  = false;
    };

    void StartFromReload(Spt& spt, uint64_t tick);

    bool     Cascaded() const;
    uint64_t Spt1Tick(uint64_t sys_tick) const;
    uint64_t Sync();
    void     Arm();
    void     OnEvent();
    void     OnRateChange();
    void     ResetState();
    void     WriteRefresh(uint32_t off, uint32_t value);
    void     WriteSpt0(uint32_t off, uint32_t value);
    void     WriteSpt1(uint32_t off, uint32_t value);

    [[noreturn]] void Reject(const char* what, uint32_t off, uint32_t value);

    GuestCycleClock* clock_           = nullptr;
    Vrc5477Intc*     intc_            = nullptr;
    OscillatorTicks  sys_{emu_, false};
    Spt              spt_[2];
    uint32_t         spt1_control_    = 0;
    uint64_t         spt0_terminals_  = 0;
    uint32_t         refresh_reload_  = 0;
    uint32_t         refresh_control_ = 0;
};

}

#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>
#include <mutex>

namespace cerfengine { class MipsJit; }

namespace cerfcore {

class StateWriter;
class StateReader;

class Vrc5477Intc : public Service {
public:
    using Service::Service;
    bool ShouldRegister() override;
    void OnReady() override;

    /* off is relative to the INTC block base (0x400). 32-bit registers. */
    uint32_t ReadReg(uint32_t off);
    void     WriteReg(uint32_t off, uint32_t value);

    void AssertSource(uint32_t irq);
    void DeassertSource(uint32_t irq);
    void LatchSource(uint32_t irq);

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);
    /* Re-drive the CPU IP lines from restored register state (the syscon calls
       this from its PostRestore, after every peripheral's RestoreState). */
    void Renotify();

private:
    /* INTnSTAT: the pending IRQs that are enabled and routed to CPU output n
       (4-bit field per IRQ in INTCTRL[irq/8]: bit3 = enable, bits[2:0] = route). */
    uint32_t StatusForLineLocked(uint32_t n) const;

    void NotifyLocked();
    void LatchLocked(uint32_t mask);

    mutable std::mutex state_mtx_;
    cerfengine::MipsJit* jit_        = nullptr;
    uint32_t intctrl_[4] = {0, 0, 0, 0};
    uint32_t intppes0_   = 0;
    uint32_t intppes1_   = 0;
    uint32_t cpustat_    = 0;
    uint32_t busctrl_    = 0;
    uint32_t nmistat_    = 0;
    uint32_t pending_    = 0;
    uint32_t line_level_ = 0;
};

}

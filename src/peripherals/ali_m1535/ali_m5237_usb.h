#pragma once

#include "../usb/ohci_host_controller.h"
#include "ali_m1535_internal_function.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

class AliM5237Usb : public AliM1535FunctionMemory, public OhciHostBus {
public:
    AliM5237Usb(CerfEmulator& emu, const AliM1535InternalFunction& function, std::function<void(bool)> interrupt);

    void Attach();
    void Reset();

    uint32_t FunctionMemRead(uint32_t offset, unsigned size) override;
    void     FunctionMemWrite(uint32_t offset, uint32_t value, unsigned size) override;

    uint32_t OhciRead32(uint32_t addr) override;
    void     OhciWrite32(uint32_t addr, uint32_t value) override;
    void     OhciInterrupt(bool asserted) override;

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);
    void PostRestore();

private:
    void RequireBusMaster(uint32_t addr);

    CerfEmulator&                   emu_;
    const AliM1535InternalFunction& function_;
    std::function<void(bool)>       interrupt_;
    OhciHostController              ohci_;
};

}

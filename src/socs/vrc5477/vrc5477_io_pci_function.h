#pragma once

#include "../../peripherals/pci/pci_bar.h"
#include "../../peripherals/pci/pci_device.h"
#include "../../peripherals/pci/pci_type0_header.h"

#include <cstdint>

namespace cerfcore {

class CerfEmulator;

class Vrc5477IoPciFunction : public PciDevice {
public:
    Vrc5477IoPciFunction(CerfEmulator& emu, uint8_t dev, uint32_t identity, uint32_t class_rev, PciBar bar0);

    void Reset();

    uint8_t PciDev() const override { return dev_; }
    uint8_t PciFnc() const override { return 0u; }

    uint32_t ConfigRead(uint32_t reg) override;
    void     ConfigWrite(uint32_t reg, uint32_t value) override;

    bool     MemClaims(uint32_t pci_addr) const override;
    uint32_t MemRead(uint32_t pci_addr, unsigned size) override;
    void     MemWrite(uint32_t pci_addr, uint32_t value, unsigned size) override;

    bool     IoClaims(uint32_t pci_io) const override;
    uint32_t IoRead(uint32_t pci_io, unsigned size) override;
    void     IoWrite(uint32_t pci_io, uint32_t value, unsigned size) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

private:
    static bool HeaderStub(uint32_t reg);
    uint32_t    Line() const;

    CerfEmulator&     emu_;
    uint8_t           dev_;
    PciHeaderIdentity identity_;
    PciType0Header    header_;
};

}

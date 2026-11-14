#pragma once

#include "../pci/pci_bar.h"
#include "../pci/pci_device.h"
#include "../pci/pci_power_management.h"
#include "../pci/pci_type0_header.h"
#include "ali_m1535_bridge_config.h"

#include <cstddef>
#include <cstdint>

namespace cerfcore {

class CerfEmulator;

struct AliM1535FunctionIdentity {
    static constexpr size_t kBars = 2;

    const char*        name;
    uint32_t           idsel_line;
    PciHeaderIdentity  header;
    PciPowerManagement pm;
    PciBar             bars[kBars];
};

class AliM1535FunctionMemory {
public:
    virtual ~AliM1535FunctionMemory() = default;

    virtual uint32_t FunctionMemRead(uint32_t offset, unsigned size)                  = 0;
    virtual void     FunctionMemWrite(uint32_t offset, uint32_t value, unsigned size) = 0;
};

class AliM1535InternalFunction : public PciDevice {
public:
    AliM1535InternalFunction(CerfEmulator& emu, const AliM1535BridgeConfig& bridge, AliM1535Function function,
                             const AliM1535FunctionIdentity& identity, AliM1535FunctionMemory* memory);

    void     Reset();
    bool     BusMasterEnabled() const;
    uint32_t IdselLine() const { return identity_.idsel_line; }
    void     SetPciDevice(uint8_t dev) { dev_ = dev; }

    uint8_t PciDev() const override { return dev_; }
    uint8_t PciFnc() const override { return 0u; }
    bool    ConfigClaims() const override { return bridge_.Visible(function_); }

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
    [[noreturn]] void Reject(const char* what, uint32_t addr, uint32_t value);

    CerfEmulator&                   emu_;
    const AliM1535BridgeConfig&     bridge_;
    AliM1535Function                function_;
    const AliM1535FunctionIdentity& identity_;
    AliM1535FunctionMemory*         memory_;
    uint8_t                         dev_ = 0;
    PciType0Header                  header_;
    PciPowerManagement              pm_;
};

}

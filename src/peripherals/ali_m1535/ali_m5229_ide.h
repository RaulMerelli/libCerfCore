#pragma once

#include "../pci/pci_device.h"
#include "../pci/pci_power_management.h"
#include "../pci/pci_type0_header.h"
#include "ali_m1535_bridge_config.h"

#include <cstddef>
#include <cstdint>

namespace cerfcore {

class CerfEmulator;

class AliM5229Ide : public PciDevice {
public:
    AliM5229Ide(CerfEmulator& emu, const AliM1535BridgeConfig& bridge, uint32_t idsel_line);

    void     Reset();
    uint32_t IdselLine() const { return idsel_line_; }
    void     SetPciDevice(uint8_t dev) { dev_ = dev; }

    uint8_t PciDev() const override { return dev_; }
    uint8_t PciFnc() const override { return 0u; }
    bool    ConfigClaims() const override { return bridge_.Visible(AliM1535Function::kIde); }

    uint32_t ConfigRead(uint32_t reg) override;
    void     ConfigWrite(uint32_t reg, uint32_t value) override;

    bool     MemClaims(uint32_t) const override { return false; }
    uint32_t MemRead(uint32_t pci_addr, unsigned size) override;
    void     MemWrite(uint32_t pci_addr, uint32_t value, unsigned size) override;

    bool     IoClaims(uint32_t pci_io) const override;
    uint32_t IoRead(uint32_t pci_io, unsigned size) override;
    void     IoWrite(uint32_t pci_io, uint32_t value, unsigned size) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

private:
    static constexpr uint32_t kDeviceRegsFirst = 0x40u;
    static constexpr uint32_t kDeviceRegsCount = 0x40u;

    uint8_t Dev(uint32_t reg) const { return dev_regs_[reg - kDeviceRegsFirst]; }
    uint8_t DeviceByteRead(uint32_t reg) const;
    uint8_t ProgIfRead() const;
    void    WriteProgIf(uint8_t value);
    void    WriteDeviceByte(uint32_t reg, uint8_t value);
    bool    Compatibility() const;

    [[noreturn]] void Reject(const char* what, uint32_t addr, uint32_t value);

    CerfEmulator&               emu_;
    const AliM1535BridgeConfig& bridge_;
    uint32_t                    idsel_line_;
    uint8_t                     dev_              = 0;
    PciType0Header              header_;
    uint8_t                     prog_if_          = 0;
    uint16_t                    subsystem_vendor_ = 0;
    uint16_t                    subsystem_device_ = 0;
    PciPowerManagement          pm_;
    uint8_t                     dev_regs_[kDeviceRegsCount] = {};
};

}

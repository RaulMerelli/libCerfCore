#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/service.h>
#include "../pci/pci_bus.h"
#include "../pci/pci_common_header.h"
#include "../pci/pci_device.h"
#include "../pci/pci_power_management.h"
#include "../pci/pci_segment.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

class PciHostBridge;

class Intel21152Bridge : public Service, public PciDevice, public PciBus {
public:
    explicit Intel21152Bridge(CerfEmulator& emu);

    bool ShouldRegister() override;
    void OnReady() override;

    uint8_t PciDev() const override { return dev_; }
    uint8_t PciFnc() const override { return 0u; }
    bool    DecodesFunction(uint32_t) const override { return true; }

    uint32_t ConfigRead(uint32_t reg) override;
    void     ConfigWrite(uint32_t reg, uint32_t value) override;

    bool     MemClaims(uint32_t pci_addr) const override;
    uint32_t MemRead(uint32_t pci_addr, unsigned size) override;
    void     MemWrite(uint32_t pci_addr, uint32_t value, unsigned size) override;

    bool     IoClaims(uint32_t pci_io) const override;
    uint32_t IoRead(uint32_t pci_io, unsigned size) override;
    void     IoWrite(uint32_t pci_io, uint32_t value, unsigned size) override;

    bool     Type1ConfigClaims(uint32_t bus) const override;
    uint32_t Type1ConfigRead(uint32_t address) override;
    void     Type1ConfigWrite(uint32_t address, uint32_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

    void RegisterPciDevice(PciDevice* dev) override;
    void RegisterBusResetListener(std::function<void()> listener) override;

    uint32_t BusMasterRead32(uint32_t pci_addr) override;
    void     BusMasterWrite32(uint32_t pci_addr, uint32_t value) override;

private:
    void ResetRegisters();
    void ResetChip(bool hold_secondary_reset);
    void WriteBridgeControl(uint16_t value);
    void WriteDeviceControl(uint32_t value);

    bool InIoWindow(uint32_t addr) const;
    bool InMemoryWindows(uint32_t addr) const;
    bool VgaIo(uint32_t addr) const;
    bool VgaMemory(uint32_t addr) const;

    void       RequireSecondaryOutOfReset(const char* what, uint32_t addr) const;
    PciDevice* SecondaryClaimer(bool (PciDevice::*claims)(uint32_t) const, uint32_t addr, const char* what) const;
    void       SecondaryMasterAbort(const char* what, uint32_t addr, uint32_t value);
    uint32_t   MasterAbortRead(const char* what, uint32_t addr);
    PciDevice* SecondaryConfigTarget(uint32_t address, const char* what) const;
    uint32_t   SecondaryConfigRegister(uint32_t address) const;

    [[noreturn]] void Reject(const char* what, uint32_t addr, uint32_t value) const;

    PciHostBridge*     primary_ = nullptr;
    uint8_t            dev_     = 0;
    PciCommonHeader    header_;
    PciPowerManagement pm_;
    PciSegment         secondary_;
    CallbackList<>     secondary_reset_;

    uint8_t  primary_bus_        = 0;
    uint8_t  secondary_bus_      = 0;
    uint8_t  subordinate_bus_    = 0;
    uint8_t  secondary_latency_  = 0;
    uint8_t  io_base_            = 0;
    uint8_t  io_limit_           = 0;
    uint16_t io_base_upper_      = 0;
    uint16_t io_limit_upper_     = 0;
    uint16_t mem_base_           = 0;
    uint16_t mem_limit_          = 0;
    uint16_t pref_base_          = 0;
    uint16_t pref_limit_         = 0;
    uint32_t pref_base_upper_    = 0;
    uint32_t pref_limit_upper_   = 0;
    uint16_t bridge_control_     = 0;
    uint8_t  chip_control_       = 0;
    uint16_t arbiter_control_    = 0;
    uint8_t  serr_event_disable_ = 0;
    uint16_t clock_control_      = 0;
    uint16_t secondary_status_   = 0;
};

}

#pragma once

#include "../../peripherals/pci/pci_host_bridge.h"
#include "vrc5477_io_pci_function.h"
#include "vrc5477_pci_channel.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

class StateReader;
class StateWriter;

enum class Vrc5477PciChannelId { kExternal, kInternal };

class Vrc5477Pci : public PciHostBridge {
public:
    explicit Vrc5477Pci(CerfEmulator& emu);

    bool ShouldRegister() override;
    void OnReady() override;

    void RegisterPciDevice(PciDevice* dev) override;
    void RegisterSubtractiveDecoder(PciDevice* dev) override;
    void RegisterBusResetListener(std::function<void()> listener) override;

    uint8_t DeviceOfIdselLine(uint32_t line) const override { return Vrc5477PciChannel::DeviceOfIdselLine(line); }

    uint32_t BusMasterRead32(uint32_t pci_addr) override;
    void     BusMasterWrite32(uint32_t pci_addr, uint32_t value) override;

    static bool Owns(uint32_t block_off);
    static bool IsBusControlHigh(uint32_t block_off);
    uint32_t    ReadReg(uint32_t block_off);
    void        WriteReg(uint32_t block_off, uint32_t value);
    uint32_t    ReadRegPartial(uint32_t block_off, unsigned size);
    void        WriteRegPartial(uint32_t block_off, uint32_t value, unsigned size);
    void        BusControlHighWritten(uint32_t block_off, uint32_t value);

    Vrc5477PciChannel& Channel(Vrc5477PciChannelId id);

    void SaveTargetState(StateWriter& w);
    void RestoreTargetState(StateReader& r);

private:
    void     ResetInternalFunctions();
    void     ResetTarget();
    bool     WriteTargetReg(uint32_t block_off, uint32_t value);
    bool     BacksSdram(uint32_t base, uint32_t size) const;
    uint32_t BusMasterPa(uint32_t pci_addr);

    uint32_t sdram_bank_[2]     = {};
    uint32_t target_command_[2] = {};
    uint32_t sdram_bar_[2]      = {};

    Vrc5477PciChannel    external_;
    Vrc5477PciChannel    internal_;
    Vrc5477IoPciFunction ac97_;
    Vrc5477IoPciFunction usb_function_;
    Vrc5477IoPciFunction usb_host_;
};

}

#pragma once

#include "../../peripherals/pci/pci_host_bridge.h"
#include "vrc5477_io_pci_function.h"
#include "vrc5477_pci_channel.h"

#include <cstdint>
#include <functional>

namespace cerfcore {

enum class Vrc5477PciChannelId { kExternal, kInternal };

class Vrc5477Pci : public PciHostBridge {
public:
    explicit Vrc5477Pci(CerfEmulator& emu);

    bool ShouldRegister() override;
    void OnReady() override;

    void RegisterPciDevice(PciDevice* dev) override;
    void RegisterBusResetListener(std::function<void()> listener) override;

    static bool Owns(uint32_t block_off);
    static bool IsBusControlHigh(uint32_t block_off);
    uint32_t    ReadReg(uint32_t block_off);
    void        WriteReg(uint32_t block_off, uint32_t value);
    void        BusControlHighWritten(uint32_t block_off, uint32_t value);

    Vrc5477PciChannel& Channel(Vrc5477PciChannelId id);

private:
    void ResetInternalFunctions();

    Vrc5477PciChannel    external_;
    Vrc5477PciChannel    internal_;
    Vrc5477IoPciFunction ac97_;
    Vrc5477IoPciFunction usb_function_;
    Vrc5477IoPciFunction usb_host_;
};

}

#pragma once

#include <cstdint>
#include <functional>

namespace cerfcore {

class PciDevice;

class PciBus {
public:
    virtual ~PciBus() = default;

    virtual void RegisterPciDevice(PciDevice* dev)                        = 0;
    virtual void RegisterBusResetListener(std::function<void()> listener) = 0;

    virtual uint32_t BusMasterRead32(uint32_t pci_addr)                 = 0;
    virtual void     BusMasterWrite32(uint32_t pci_addr, uint32_t value) = 0;
};

}

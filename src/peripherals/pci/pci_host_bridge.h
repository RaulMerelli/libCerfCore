#pragma once

#include <cerfcore/core/service.h>
#include "pci_bus.h"

#include <cstdint>

namespace cerfcore {

class PciDevice;

class PciHostBridge : public Service, public PciBus {
public:
    using Service::Service;

    virtual void RegisterSubtractiveDecoder(PciDevice* dev) = 0;

    virtual uint8_t DeviceOfIdselLine(uint32_t line) const = 0;
};

}

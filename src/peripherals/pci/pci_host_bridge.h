#pragma once

#include <cerfcore/core/service.h>

#include <functional>

namespace cerfcore {

class PciDevice;

class PciHostBridge : public Service {
public:
    using Service::Service;

    virtual void RegisterPciDevice(PciDevice* dev) = 0;
    virtual void RegisterBusResetListener(std::function<void()> listener) = 0;
};

}

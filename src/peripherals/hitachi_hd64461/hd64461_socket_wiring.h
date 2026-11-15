#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_object.h>

#include <cstdint>

namespace cerfcore {

class Hd64461SocketWiring : public Service {
public:
    using Service::Service;

    virtual bool           SocketPowered(uint32_t channel, bool vccsel0, bool vccsel1) const = 0;
    virtual ObjectIdentity Socket(uint32_t channel) const                                   = 0;
    virtual uint16_t       ModePins() const                                                 = 0;
};

}

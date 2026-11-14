#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class AliM1535Board : public Service {
public:
    using Service::Service;

    virtual uint32_t BridgeIdselLine() const      = 0;
    virtual void     SetInterruptLevel(bool high) = 0;

    virtual uint32_t ExternalIsaIoRead(uint32_t port, unsigned size)                  = 0;
    virtual void     ExternalIsaIoWrite(uint32_t port, uint32_t value, unsigned size) = 0;
};

}

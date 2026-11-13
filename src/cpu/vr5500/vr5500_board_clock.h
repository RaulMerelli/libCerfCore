#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Vr5500BoardClock : public Service {
public:
    using Service::Service;

    virtual uint64_t PClockHz() const = 0;
    virtual uint32_t DivMode() const = 0;
};

}

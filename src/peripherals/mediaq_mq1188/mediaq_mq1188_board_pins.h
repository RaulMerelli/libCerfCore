#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class MediaQMq1188BoardPins : public Service {
public:
    using Service::Service;
    ~MediaQMq1188BoardPins() override = default;

    virtual void SetInterruptLevel(bool high) = 0;
    virtual bool GpioInputHigh(uint32_t gpio) = 0;
};

}

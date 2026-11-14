#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

enum class Hp600LedColor : uint8_t { kOrange, kRed, kGreen };

class Hp600LedColors : public Service {
public:
    using Service::Service;

    virtual Hp600LedColor ChargingColor() const = 0;
    virtual Hp600LedColor NotificationColor() const = 0;
};

}

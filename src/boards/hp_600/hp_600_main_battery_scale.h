#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Hp600MainBatteryScale : public Service {
public:
    using Service::Service;

    virtual uint16_t LevelThreshold(int level) const = 0;
};

}

#pragma once

#include "machine_object.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

struct TemperatureReading {
    bool                   reported = false;
    std::optional<int16_t> half_celsius;
};

class MachineTemperatureSensor : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kTemperatureSensor; }

    virtual TemperatureReading Reading() const                         = 0;
    virtual void               SetReading(const TemperatureReading& r) = 0;
};

}

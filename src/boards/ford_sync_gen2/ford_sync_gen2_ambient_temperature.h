#pragma once
#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_temperature_sensor.h>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace cerfcore {

class StateWriter;
class StateReader;

class FordSyncGen2AmbientTemperature : public Service, public MachineTemperatureSensor {
public:
    using Service::Service;
    struct Snapshot {
        bool available = false;
        std::optional<int16_t> half_celsius;
        uint64_t revision = 0;
    };
    bool ShouldRegister() override;
    Snapshot Read() const;

    std::string        ObjectId() const override { return "outside_temperature"; }
    static constexpr char kObjectName[] = "Outside temperature";
    std::string        ObjectName() const override { return kObjectName; }
    TemperatureReading Reading() const override;
    void               SetReading(const TemperatureReading& reading) override;

    void Set(std::optional<int16_t> half_celsius);
    void Clear();
    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);
private:
    mutable std::mutex mutex_;
    Snapshot state_;
};

}

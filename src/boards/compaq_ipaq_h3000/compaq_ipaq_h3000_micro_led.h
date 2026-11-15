#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_indicators.h>

#include <cstdint>
#include <mutex>

namespace cerfcore {

class CompaqIpaqH3000Battery;
class StateReader;
class StateWriter;

class CompaqIpaqH3000MicroLed : public Service, public MachineLamp {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void SetLed(const uint8_t* req, uint8_t len);
    void PowerOn();

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

    std::string ObjectId() const override { return "alarm_charge_led"; }
    static constexpr char kObjectName[] = "Alarm/charge LED";
    std::string ObjectName() const override { return kObjectName; }
    LampState   Lamp() const override;

private:
    struct Setting {
        uint8_t color    = 0;
        uint8_t flags    = 0;
        uint8_t on_time  = 0;
        uint8_t off_time = 0;
    };

    Setting Current() const;

    mutable std::mutex      mtx_;
    Setting                 led_{};
    CompaqIpaqH3000Battery* battery_ = nullptr;
};

}

#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_indicators.h>

#include <mutex>

namespace cerfcore {

class NecMobilepro900SeriesLed : public Service, public MachineLamp {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void SetBlinking(bool blinking);
    bool Blinking() const;

    std::string ObjectId() const override { return "notification_led"; }
    static constexpr char kObjectName[] = "Notification LED";
    std::string ObjectName() const override { return kObjectName; }
    LampState   Lamp() const override;

private:
    mutable std::mutex mtx_;
    bool               blinking_ = false;
};

}

#pragma once

#include <cerfcore/core/service.h>

#include "../../host/battery_widget.h"

#include <cstdint>

namespace cerfcore {

class Hp600InputNets;
class Hp600MainBatteryScale;

class Hp600Battery : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    bool     Charging() const;
    bool     OnExternalPower() const;
    uint16_t MainBatteryAdc() const;

private:
    void DriveAcLine();

    BatteryWidget          battery_;
    Hp600InputNets*        nets_    = nullptr;
    Hp600MainBatteryScale* scale_   = nullptr;
};

}

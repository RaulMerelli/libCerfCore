#pragma once

#include <cerfcore/core/service.h>

#include <cerfcore/host/battery.h>

#include <cstdint>
#include <functional>
#include <utility>

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
    void     AddChangeListener(std::function<void()> cb) { battery_.AddChangeListener(std::move(cb)); }

private:
    void DriveAcLine();

    Battery                battery_{emu_};
    Hp600InputNets*        nets_    = nullptr;
    Hp600MainBatteryScale* scale_   = nullptr;
};

}

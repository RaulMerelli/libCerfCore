#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

namespace cerfcore {

class CasioPocketpostpetBattery : public Service {
public:
    explicit CasioPocketpostpetBattery(CerfEmulator& e) : Service(e) {}

    bool ShouldRegister() override;
    void OnReady() override;

    int  FillPercent() const { return battery_.ChargePercent(); }
    bool IsOnBattery() const { return battery_.OnBattery(); }

private:
    void DrivePowerPins();

    Battery       battery_{emu_};
};

}

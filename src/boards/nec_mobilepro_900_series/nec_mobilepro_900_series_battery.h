#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

#include <cstdint>

namespace cerfcore {

class NecMobilepro900SeriesBattery : public Service {
public:
    explicit NecMobilepro900SeriesBattery(CerfEmulator& e) : Service(e) {}

    bool ShouldRegister() override;
    void OnReady() override;

private:
    void DriveState();
    uint16_t MainBatteryRaw(int fill_percent, bool on_ac) const;

    Battery       battery_{emu_};
};

}

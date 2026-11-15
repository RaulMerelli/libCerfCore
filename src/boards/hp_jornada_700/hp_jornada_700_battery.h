#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

namespace cerfcore {

class HpJornada700Battery : public Service {
public:
    explicit HpJornada700Battery(CerfEmulator& e) : Service(e) {}

    bool ShouldRegister() override;
    void OnReady() override;

    int FillPercent() const { return battery_.ChargePercent(); }

private:
    void DriveAcPins();

    Battery       battery_{emu_};
};

}

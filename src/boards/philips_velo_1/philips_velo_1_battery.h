#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

namespace cerfcore {

class PhilipsVelo1Battery : public Service {
public:
    explicit PhilipsVelo1Battery(CerfEmulator& e) : Service(e) {}
    bool ShouldRegister() override;
    void OnReady() override;

    int FillPercent() const { return battery_.ChargePercent(); }

private:
    Battery       battery_{emu_};
};

}

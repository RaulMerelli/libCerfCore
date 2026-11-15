#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

#include <cstddef>
#include <cstdint>

namespace cerfcore {

struct NecBatteryAdcStep {
    uint16_t word;
    int      percent;
};

class NecVrc4172Battery : public Service {
public:
    using Service::Service;

    void OnReady() override;

    int  FillPercent() const { return battery_.ChargePercent(); }
    bool IsOnBattery() const { return battery_.OnBattery(); }

    template <size_t N>
    uint16_t AdcWord(const NecBatteryAdcStep (&table)[N]) const {
        const int fill = FillPercent();
        for (const NecBatteryAdcStep& step : table) {
            if (fill >= step.percent) return step.word;
        }
        return table[N - 1u].word;
    }

private:
    void DriveAcPin();

    Battery       battery_{emu_};
};

}

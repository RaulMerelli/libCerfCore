#pragma once

#include "../core/service.h"
#include "widget_control.h"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace cerfcore {

class DeviceButtons : public Service {
public:
    using Service::Service;
    ~DeviceButtons() override = default;

    std::vector<ActionControl> Actions();

protected:
    virtual std::vector<ActionControl> BoardActions() = 0;

    struct RawTapButton {
        const char* id;
        const char* label;
        uint16_t    adc_y;
    };

    std::vector<ActionControl> RawTapColumn(std::span<const RawTapButton> buttons, uint16_t adc_x,
                                            std::function<void(uint16_t, uint16_t)> tap) const;
};

}

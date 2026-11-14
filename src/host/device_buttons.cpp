#include <cerfcore/host/device_buttons.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/host_pointer_input_lock.h>

#include <utility>

namespace cerfcore {

std::vector<ActionControl> DeviceButtons::Actions() {
    HostPointerInputLock*      lock    = &emu_.Get<HostPointerInputLock>();
    std::vector<ActionControl> actions = BoardActions();
    for (ActionControl& action : actions)
        action.press = [lock, press = std::move(action.press)] { lock->RunUnlessHeld(press); };
    return actions;
}

std::vector<ActionControl> DeviceButtons::RawTapColumn(std::span<const RawTapButton> buttons, uint16_t adc_x,
                                                       std::function<void(uint16_t, uint16_t)> tap) const {
    std::vector<ActionControl> out;
    for (const RawTapButton& b : buttons) {
        ActionControl it;
        it.id    = b.id;
        it.label = b.label;
        it.press = [tap, adc_x, y = b.adc_y] { tap(adc_x, y); };
        out.push_back(std::move(it));
    }
    return out;
}

}

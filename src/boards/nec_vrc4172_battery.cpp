#include "nec_vrc4172_battery.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include "../peripherals/nec_vrc4172/vrc4172_gpio.h"

namespace cerfcore {

namespace {

constexpr int kOffAcPin = 5;

}

void NecVrc4172Battery::OnReady() {
    emu_.Get<MachineObjectTree>().Add(&battery_);
    battery_.AddChangeListener([this] { DriveAcPin(); });
    DriveAcPin();
}

void NecVrc4172Battery::DriveAcPin() {
    emu_.Get<Vrc4172Gpio>().SetPinLevel(kOffAcPin, battery_.OnBattery());
}

}

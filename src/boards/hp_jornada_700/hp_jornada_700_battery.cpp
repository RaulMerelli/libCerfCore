#include "hp_jornada_700_battery.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include "../../socs/sa11xx/sa11xx_gpio.h"
#include <cerfcore/boards/board_context.h>
#include "hp_jornada_700_family_id.h"

namespace cerfcore {

bool HpJornada700Battery::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetBoardFamilyId() == BoardFamilyId::HpJornada700;
}

void HpJornada700Battery::OnReady() {
    battery_.AddChangeListener([this] { DriveAcPins(); });
    emu_.Get<MachineObjectTree>().Add(&battery_);
    DriveAcPins();   /* publish the initial AC/charging pin state */
}

void HpJornada700Battery::DriveAcPins() {
    const bool on_batt = battery_.OnBattery();
    auto& gpio = emu_.Get<Sa11xxGpio>();
    gpio.DriveInputPin(4, on_batt);    /* GPIO4 high  -> ACLineStatus offline */
    gpio.DriveInputPin(26, on_batt);   /* GPIO26 high -> not charging          */
}

REGISTER_SERVICE(HpJornada700Battery);

}

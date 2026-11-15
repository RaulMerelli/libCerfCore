#include "nec_mobilepro_900_series_led.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/boards/board_context.h>
#include "nec_mobilepro_900_series_family_id.h"

namespace cerfcore {

REGISTER_SERVICE(NecMobilepro900SeriesLed);

namespace {
constexpr uint32_t  kBlinkHalfMs = 500u;
constexpr ObjectRgb kLedColor{120, 255, 140};
}

bool NecMobilepro900SeriesLed::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetBoardFamilyId() == BoardFamilyId::NecMobilepro900Series;
}

void NecMobilepro900SeriesLed::OnReady() {
    emu_.Get<MachineObjectTree>().Add(this);
}

void NecMobilepro900SeriesLed::SetBlinking(bool blinking) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        blinking_ = blinking;
    }
    Refresh();
}

bool NecMobilepro900SeriesLed::Blinking() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return blinking_;
}

LampState NecMobilepro900SeriesLed::Lamp() const {
    const bool blinking = Blinking();
    return LampState{blinking, blinking, kLedColor, blinking ? kBlinkHalfMs : 0u, blinking ? kBlinkHalfMs : 0u};
}

}

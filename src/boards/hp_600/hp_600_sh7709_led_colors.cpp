#include "hp_600_led_colors.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7709/sh7709_id.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

class Hp600Sh7709LedColors : public Hp600LedColors {
public:
    using Hp600LedColors::Hp600LedColors;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7709;
    }

    Hp600LedColor ChargingColor() const override { return Hp600LedColor::kOrange; }
    Hp600LedColor NotificationColor() const override { return Hp600LedColor::kOrange; }
};

}

REGISTER_SERVICE_AS(Hp600Sh7709LedColors, Hp600LedColors);

}

#include "hp_600_main_battery_scale.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7709/sh7709_id.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kLevelThresholds[] = {0x286u, 0x2C7u, 0x312u, 0x332u, 0x345u, 0x35Bu, 0x373u};

class Hp600Sh7709MainBatteryScale : public Hp600MainBatteryScale {
public:
    using Hp600MainBatteryScale::Hp600MainBatteryScale;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7709;
    }

    uint16_t LevelThreshold(int level) const override { return kLevelThresholds[level]; }
};

}

REGISTER_SERVICE_AS(Hp600Sh7709MainBatteryScale, Hp600MainBatteryScale);

}

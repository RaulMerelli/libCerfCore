#include "hp_600_main_battery_scale.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7729/sh7729_id.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kLevelThresholds[] = {0x2DCu, 0x316u, 0x342u, 0x34Bu, 0x35Eu, 0x36Cu, 0x389u};

class Hp600Sh7729MainBatteryScale : public Hp600MainBatteryScale {
public:
    using Hp600MainBatteryScale::Hp600MainBatteryScale;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7729;
    }

    uint16_t LevelThreshold(int level) const override { return kLevelThresholds[level]; }
};

}

REGISTER_SERVICE_AS(Hp600Sh7729MainBatteryScale, Hp600MainBatteryScale);

}

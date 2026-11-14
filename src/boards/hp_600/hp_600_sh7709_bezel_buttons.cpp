#include "hp_600_bezel_buttons.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7709/sh7709_id.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

class Hp600Sh7709BezelButtons : public Hp600BezelButtons {
public:
    using Hp600BezelButtons::Hp600BezelButtons;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7709;
    }

    const std::array<Hp600BezelName, kHp600BezelZones>& Names() const override { return kNames; }

private:
    static constexpr std::array<Hp600BezelName, kHp600BezelZones> kNames = {{
        {"settings", "Settings"},
        {"mail", "Mail"},
        {"fax", "Fax"},
        {"find", "Find"},
    }};
};

}

REGISTER_SERVICE_AS(Hp600Sh7709BezelButtons, Hp600BezelButtons);

}

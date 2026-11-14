#include "hp_600_bezel_buttons.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7729/sh7729_id.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

class Hp600Sh7729BezelButtons : public Hp600BezelButtons {
public:
    using Hp600BezelButtons::Hp600BezelButtons;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7729;
    }

    const std::array<Hp600BezelName, kHp600BezelZones>& Names() const override { return kNames; }

private:
    static constexpr std::array<Hp600BezelName, kHp600BezelZones> kNames = {{
        {"settings", "Settings"},
        {"page_up", "Page Up"},
        {"page_down", "Page Down"},
        {"task_switch", "Task Switch"},
    }};
};

}

REGISTER_SERVICE_AS(Hp600Sh7729BezelButtons, Hp600BezelButtons);

}

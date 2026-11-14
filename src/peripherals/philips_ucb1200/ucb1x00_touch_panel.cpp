#include "ucb1x00_touch_panel.h"

#include "ucb1x00_codec.h"

#include <cerfcore/boards/board_context.h>
#include "../../boards/philips_nino/philips_nino_family_id.h"
#include "../../boards/philips_velo_1/philips_velo_1_family_id.h"
#include "../../boards/sharp_mobilon_hc/sharp_mobilon_hc_family_id.h"
#include "../../boards/siemens_simpad/siemens_simpad_family_id.h"
#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

bool Ucb1x00TouchPanel::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    if (!bd) return false;
    const std::string_view b = bd->GetBoardFamilyId();
    return b == BoardFamilyId::PhilipsNino || b == BoardFamilyId::PhilipsVelo1 ||
           b == BoardFamilyId::SharpMobilonHc || b == BoardFamilyId::SiemensSimpad;
}

void Ucb1x00TouchPanel::PenDown(int x, int y) { SetPen(true, x, y); }

void Ucb1x00TouchPanel::PenMove(int x, int y) {
    if (Down()) SetPen(true, x, y);
}

void Ucb1x00TouchPanel::PenUp(int x, int y) { SetPen(false, x, y); }

void Ucb1x00TouchPanel::CaptureLost() { SetPen(false, 0, 0); }

bool Ucb1x00TouchPanel::Down() { return emu_.Get<Ucb1x00Codec>().PenDown(); }

void Ucb1x00TouchPanel::SetPen(bool down, int x, int y) {
    x_.store(x, std::memory_order_relaxed);
    y_.store(y, std::memory_order_relaxed);
    emu_.Get<Ucb1x00Codec>().SetTouchPressed(down);
}

REGISTER_SERVICE_AS(Ucb1x00TouchPanel, TouchInput);

}

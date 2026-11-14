#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/touch_input.h>
#include "hp_600_family_id.h"
#include "hp_600_touch_panel.h"

namespace cerfcore {

namespace {

class Hp600TouchInput : public TouchInput {
public:
    using TouchInput::TouchInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override { panel_ = &emu_.Get<Hp600TouchPanel>(); }

    void OnPenDown(int x, int y) override { panel_->PenDown(x, y); }
    void OnPenMove(int x, int y) override { panel_->PenMove(x, y); }
    void OnPenUp(int, int) override { panel_->PenUp(); }
    void OnCaptureLost() override { panel_->PenUp(); }

private:
    Hp600TouchPanel* panel_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp600TouchInput, TouchInput);

}

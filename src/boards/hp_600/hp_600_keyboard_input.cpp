#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_input.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/host/keyboard_router.h>
#include "hp_600_family_id.h"
#include "hp_600_key_matrix.h"

#include <cstdint>

namespace cerfcore {

namespace {

class Hp600KeyboardInput : public KeyboardInput {
public:
    using KeyboardInput::KeyboardInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        map_    = &emu_.Get<KeyboardMap>();
        matrix_ = &emu_.Get<Hp600KeyMatrix>();
        emu_.Get<KeyboardRouter>().Register(this);
    }

    void OnHostKey(uint8_t vk, bool key_up) override {
        uint32_t cell = 0u;
        if (!map_->BaseDeviceCode(vk, cell)) return;
        matrix_->SetKey(cell, !key_up);
    }

private:
    KeyboardMap*    map_    = nullptr;
    Hp600KeyMatrix* matrix_ = nullptr;
};

}

REGISTER_SERVICE(Hp600KeyboardInput);

}

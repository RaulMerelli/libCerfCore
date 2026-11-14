#include "../../peripherals/intel_21152/intel_21152_board.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_sg2_vr5500_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBridgeIdselLine = 16u;

class NecSg2Vr5500Intel21152Board : public Intel21152Board {
public:
    using Intel21152Board::Intel21152Board;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecSg2Vr5500;
    }

    uint32_t BridgeIdselLine() const override { return kBridgeIdselLine; }
};

}

REGISTER_SERVICE_AS(NecSg2Vr5500Intel21152Board, Intel21152Board);

}

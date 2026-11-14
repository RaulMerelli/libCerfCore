#include "hp_600_input_nets.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7709/sh7709_id.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

class Hp600Sh7709InputNets : public Hp600InputNets {
public:
    using Hp600InputNets::Hp600InputNets;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7709;
    }

protected:
    std::span<const Landing, kNetCount> Landings() const override { return kLandings; }
    uint8_t UndrivenInputs(uint32_t) const override { return 0u; }

private:
    static constexpr Landing kLandings[] = {
        {Landing::Kind::kIntcPin, Sh77xxIntcPin::kIrq1, 0u, 0u},
        {Landing::Kind::kIntcPin, Sh77xxIntcPin::kIrq5, kSh77xxPortSc, 0x80u},
        {Landing::Kind::kPortPin, Sh77xxIntcPin::kIrq0, kSh77xxPortG, 0x08u},
        {},
        {},
        {},
    };
};

}

REGISTER_SERVICE_AS(Hp600Sh7709InputNets, Hp600InputNets);

}

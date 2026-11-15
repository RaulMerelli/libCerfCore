#include "../../peripherals/hitachi_hd64461/hd64461_socket_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

constexpr char kPcCardSlotId[]    = "pc_card_slot";
constexpr char kPcCardSlotLabel[] = "PC Card slot";
constexpr char kCfSlotId[]        = "cf_slot";
constexpr char kCfSlotLabel[]     = "CF slot";

class Hp600Hd64461SocketWiring : public Hd64461SocketWiring {
public:
    using Hd64461SocketWiring::Hd64461SocketWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    bool SocketPowered(uint32_t channel, bool vccsel0, bool vccsel1) const override {
        if (channel == 0u) return !vccsel1;
        return !vccsel0 && !vccsel1;
    }

    ObjectIdentity Socket(uint32_t channel) const override {
        if (channel == 0u) return {kPcCardSlotId, kPcCardSlotLabel};
        return {kCfSlotId, kCfSlotLabel};
    }

    uint16_t ModePins() const override { return 0u; }
};

}

REGISTER_SERVICE_AS(Hp600Hd64461SocketWiring, Hd64461SocketWiring);

}

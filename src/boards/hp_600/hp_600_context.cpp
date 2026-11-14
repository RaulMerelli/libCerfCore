#include <cerfcore/boards/board_context.h>

#include "hp_600_family_id.h"
#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

namespace {

class Hp600Context : public BoardContext {
public:
    using BoardContext::BoardContext;

    std::string_view GetBoardFamilyId() const override { return BoardFamilyId::Hp600; }
    uint32_t         GuestAdditionsWindowBase() const override { return 0x09000000u; }
};

}

REGISTER_SERVICE_AS(Hp600Context, BoardContext);

}

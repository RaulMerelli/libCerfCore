#include "../sh77xx/sh77xx_dmac.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sh7729_id.h"

namespace cerfcore {

namespace {

class Sh7729Dmac : public Sh77xxDmac {
public:
    using Sh77xxDmac::Sh77xxDmac;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7729; }

protected:
    bool ChannelsHeldInStandby() const override { return true; }
};

}

REGISTER_SERVICE_AS(Sh7729Dmac, Sh77xxDmac);

}

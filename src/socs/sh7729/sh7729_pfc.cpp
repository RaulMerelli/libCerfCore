#include "../sh77xx/sh77xx_pfc_impl.h"
#include "sh7729_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kSh7729ControlReset[kSh77xxPortCount] = {
    0x0000u, 0x0000u, 0xAAAAu, 0xAA8Au, 0xAAAAu, 0xAAAAu, 0xAAAAu, 0xAAAAu, 0x0000u, 0x0000u, 0x0000u, 0xA888u,
};

constexpr cerf_reg_window_detail::RegWindowModel kSh7729PfcControl =
    sh77xx_pfc::ControlWindow(kSh7729ControlReset, 1u << kSh77xxPortG);

class Sh7729Pfc : public sh77xx_pfc::Sh77xxPfcBase<SocId::Sh7729, kSh7729PfcControl> {
public:
    using Sh77xxPfcBase::Sh77xxPfcBase;
};

}

REGISTER_SERVICE_AS(Sh7729Pfc, Sh77xxPfc);

}

#include "../sh77xx/sh77xx_power_down_impl.h"
#include "../sh77xx/sh77xx_standby_control.h"
#include "sh7729_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kSh7729Stbcr2 = sh77xx_power_down::Stbcr2(
    0x7Fu, 0xC0u, cerf_reg_window_detail::OtherReset::kRetain);

constexpr uint8_t kSh7729Stbcr2ModuleStops = 0x3Fu;

class Sh7729Stbcr2
    : public sh77xx_power_down::ModuleStopRegister<SocId::Sh7729, kSh7729Stbcr2, Sh77xxStbcr2,
                                                   kSh7729Stbcr2ModuleStops> {
public:
    using ModuleStopRegister::ModuleStopRegister;
};

}

REGISTER_SERVICE_AS(Sh7729Stbcr2, Sh77xxStbcr2);

}

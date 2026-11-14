#include "../sh77xx/sh77xx_dac_impl.h"
#include "sh7729_id.h"

namespace cerfcore {

namespace {

constexpr cerf_reg_window_detail::RegWindowModel kSh7729Dac =
    sh77xx_dac::WindowOf(cerf_reg_window_detail::OtherReset::kReset);

class Sh7729Dac : public sh77xx_dac::Sh77xxDacBase<SocId::Sh7729, kSh7729Dac> {
public:
    using Sh77xxDacBase::Sh77xxDacBase;
};

}

REGISTER_SERVICE_AS(Sh7729Dac, Sh77xxDac);

}

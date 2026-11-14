#include <cerfcore/peripherals/cerf_virt/cerf_virt_config_apply.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/host/host_presentation.h>
#include <cerfcore/peripherals/cerf_virt/cerf_virt_customizations_reset.h>
#include <cerfcore/peripherals/cerf_virt/cerf_virt_framebuffer.h>
#include <cerfcore/peripherals/cerf_virt/cerf_virt_resize.h>
#include <cerfcore/state/shutdown_action.h>

namespace cerfcore {

REGISTER_SERVICE(CerfVirtConfigApply);

bool CerfVirtConfigApply::ShouldRegister() { return emu_.Get<DeviceConfig>().guest_additions; }

void CerfVirtConfigApply::Apply(GuestReset reset, uint32_t prev_w, uint32_t prev_h) {
    emu_.Get<CerfVirtCustomizationsReset>().Invalidate();
    const DeviceConfig& cfg = emu_.Get<DeviceConfig>();
    const uint32_t      w   = cfg.board_configurable_screen_width;
    const uint32_t      h   = cfg.board_configurable_screen_height;
    switch (reset) {
        case GuestReset::kSoft:
            ShowResolution(w, h);
            emu_.Get<ShutdownAction>().Perform(ShutdownChoice::SoftReset);
            return;
        case GuestReset::kHard:
            ShowResolution(w, h);
            emu_.Get<ShutdownAction>().Perform(ShutdownChoice::HardReset);
            return;
        case GuestReset::kNone:
            if (w == prev_w && h == prev_h) return;
            if (!emu_.Get<CerfVirtFramebuffer>().FitsPrimary(w, h, "live customization")) return;
            emu_.Get<HostPresentation>().FitGuestResolution(w, h);
            emu_.Get<CerfVirtResize>().RequestResize(w, h);
            return;
    }
}

void CerfVirtConfigApply::ShowResolution(uint32_t w, uint32_t h) {
    if (emu_.Get<CerfVirtFramebuffer>().SetHostMode(w, h)) emu_.Get<HostPresentation>().FitGuestResolution(w, h);
}

}

#include <cerfcore/core/service.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "../../socs/vrc5477/vrc5477_pci.h"
#include "../../socs/vrc5477/vrc5477_pdar.h"
#include "nec_sg2_vr5500_dram.h"
#include "nec_sg2_vr5500_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kOffSdram0 = 0x000u;
constexpr uint32_t kSdram0LoaderPdar =
    Vrc5477Pdar::Window32(NecSg2Vr5500Dram::kPaBase, NecSg2Vr5500Dram::kSize, true).value;

class NecSg2Vr5500BootLoader : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecSg2Vr5500;
    }

    void OnReady() override {
        pci_ = &emu_.Get<Vrc5477Pci>();
        Apply();
        emu_.Get<GuestCpuReset>().RegisterResetReleaseListener([this] { Apply(); });
    }

private:
    void Apply() { pci_->WriteReg(kOffSdram0, kSdram0LoaderPdar); }

    Vrc5477Pci* pci_ = nullptr;
};

REGISTER_SERVICE(NecSg2Vr5500BootLoader);

}

}

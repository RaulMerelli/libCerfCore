#include "siemens_simatic_mp377_power_fail.h"

#include "siemens_simatic_mp377_power_reset.h"

#include <cerfcore/boards/board_context.h>
#include "siemens_simatic_mp377_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/emulation_freeze.h>
#include "../../socs/irq_controller.h"

namespace cerfcore {

namespace siemens_simatic_mp377 {

bool SiemensSimaticMp377PowerFail::ShouldRegister() {
    auto* board = emu_.TryGet<BoardContext>();
    return board && board->GetBoardFamilyId() == BoardFamilyId::SiemensSimaticMp377;
}

void SiemensSimaticMp377PowerFail::OnReady() {
    Reset();
    emu_.Get<MachineObjectTree>().Add(this);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
        if (!emu_.Get<GuestCpuReset>().DeliveredResetWasResume()) Reset();
    });
}

void SiemensSimaticMp377PowerFail::SetPowerFailed(bool failed) {
    {
        auto frozen = emu_.Get<EmulationFreeze>().WorkerSection();
        asserted_.store(failed, std::memory_order_release);
        RefreshIrq();
    }
    Changed();
}

void SiemensSimaticMp377PowerFail::Reset() {
    asserted_.store(false, std::memory_order_release);
    RefreshIrq();
}

void SiemensSimaticMp377PowerFail::RefreshIrq() {
    auto& irq = emu_.Get<IrqController>();
    if (asserted_.load(std::memory_order_acquire))
        irq.AssertIrq(static_cast<int>(kMp377PowerFailIrqSource));
    else
        irq.DeAssertIrq(static_cast<int>(kMp377PowerFailIrqSource));
}

REGISTER_SERVICE(SiemensSimaticMp377PowerFail);

} // namespace siemens_mp377

}

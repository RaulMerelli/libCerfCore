#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include "s3c2410_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../cpu/physical_bus.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../peripherals/usb/ohci_host_controller.h"
#include "../irq_controller.h"
#include "s3c2410_held_source_levels.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>

#include <atomic>
#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x49000000u;
constexpr uint32_t kSize = 0x0000005Cu;

constexpr int kIntUsbh = 26;

constexpr OhciProfile kS3c2410Profile = {
    "S3C2410",
    0x00000010u,
    0xC000007Fu,
    0x000007FFu,
    0x00000002u,
    0xFF001B00u,
    0x00000628u,
};

class S3C2410UsbHost : public Peripheral, public OhciHostBus, public S3C2410HeldLevelSource {
public:
    explicit S3C2410UsbHost(CerfEmulator& emu) : Peripheral(emu), ohci_(emu, kS3c2410Profile, *this) {}

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetSocId() == SocId::S3c2410;
    }

    void OnReady() override {
        ohci_.Attach();
        emu_.Get<S3C2410HeldSourceLevels>().RegisterMain(kIntUsbh, this);
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { ohci_.HardwareReset(); });
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    uint32_t ReadWord(uint32_t addr) override {
        if (addr & 3u) HaltUnsupportedAccess("ReadWord(unaligned)", addr, 0);
        return ohci_.Read(addr - kBase);
    }

    void WriteWord(uint32_t addr, uint32_t value) override {
        if (addr & 3u) HaltUnsupportedAccess("WriteWord(unaligned)", addr, value);
        ohci_.Write(addr - kBase, value);
    }

    uint32_t OhciRead32(uint32_t addr) override {
        uint32_t value = 0u;
        if ((addr & 3u) != 0u || !emu_.Get<PhysicalBus>().Read(addr, MmioWidth::kWord, &value)) RejectBusMaster(addr);
        return value;
    }

    void OhciWrite32(uint32_t addr, uint32_t value) override {
        if ((addr & 3u) != 0u || !emu_.Get<PhysicalBus>().Write(addr, MmioWidth::kWord, value)) RejectBusMaster(addr);
    }

    void OhciInterrupt(bool asserted) override {
        const bool was = level_.exchange(asserted, std::memory_order_acq_rel);
        if (asserted && !was) emu_.Get<IrqController>().AssertIrq(kIntUsbh);
    }

    bool LevelAsserted(int) override { return level_.load(std::memory_order_acquire); }

    void SaveState(StateWriter& w) override { ohci_.SaveState(w); }
    void RestoreState(StateReader& r) override { ohci_.RestoreState(r); }
    void PostRestore() override { ohci_.PostRestore(); }

private:
    [[noreturn]] void RejectBusMaster(uint32_t addr) {
        emu_.Get<Fatal>().Die("S3C2410 OHCI: bus master access to an unbacked or unaligned address pa=0x%08X", addr);
    }

    OhciHostController ohci_;
    std::atomic<bool>  level_{false};
};

}

REGISTER_SERVICE(S3C2410UsbHost);

}

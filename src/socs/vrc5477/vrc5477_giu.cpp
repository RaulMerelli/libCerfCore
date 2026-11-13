#include "vrc5477_giu.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../cpu/vr5500/vr5500_id.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/state/state_stream.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "vrc5477_intc.h"

namespace cerfcore {

namespace {

/* Linux ddb5477.h: DDB_GIUFUNSEL at 0x4040 of the VRC5477 register window,
   VRC5477_IRQ_GPIO is controller source 26. */
constexpr uint32_t kBase         = 0x1FA04000u;
constexpr uint32_t kSize         = 0x00000200u;
constexpr uint32_t kOffIntClear  = 0x10u;
constexpr uint32_t kClearAll     = 0xFFFFFFFFu;
constexpr uint32_t kIntcSourceId = 26u;

constexpr uint32_t kStoredOffsets[] = {0x00u, 0x08u, 0x10u, 0x18u, 0x20u, 0x28u, 0x30u, 0x40u};

static_assert(sizeof(kStoredOffsets) / sizeof(kStoredOffsets[0]) == Vrc5477Giu::kStoredRegs);

int StoredSlot(uint32_t off) {
    for (size_t i = 0; i < Vrc5477Giu::kStoredRegs; ++i) {
        if (kStoredOffsets[i] == off) return static_cast<int>(i);
    }
    return -1;
}

}

REGISTER_SERVICE(Vrc5477Giu);

bool Vrc5477Giu::ShouldRegister() {
    return emu_.Get<BoardContext>().GetSocId() == SocId::Vr5500;
}

void Vrc5477Giu::OnReady() {
    intc_ = &emu_.Get<Vrc5477Intc>();
    GuestCpuReset& reset = emu_.Get<GuestCpuReset>();
    reset.RegisterResetListener([this](ResetLineKind kind) {
        latched_ = false;
        if (kind != ResetLineKind::Rtc) return;
        stored_written_ = false;
        for (uint32_t& v : stored_) v = 0u;
    });
    reset.RegisterResetReleaseListener([this] { DriveSource(); });
    emu_.Get<PeripheralDispatcher>().Register(this);
}

uint32_t Vrc5477Giu::MmioBase() const { return kBase; }
uint32_t Vrc5477Giu::MmioSize() const { return kSize; }

void Vrc5477Giu::WriteWord(uint32_t addr, uint32_t value) {
    const uint32_t off = addr - kBase;
    if (off == kOffIntClear && value == kClearAll) {
        latched_ = false;
        DriveSource();
        return;
    }
    const int slot = StoredSlot(off);
    if (slot < 0) HaltUnsupportedAccess("WriteWord", addr, value);
    if (input_ || latched_ || line_) {
        emu_.Get<Fatal>().Die("VRC5477 GIU write off=0x%02X val=0x%08X while the pin interrupt "
                              "is active (input=%d latched=%d line=%d)", off, value,
                              input_ ? 1 : 0, latched_ ? 1 : 0, line_ ? 1 : 0);
    }
    stored_[slot]   = value;
    stored_written_ = true;
}

void Vrc5477Giu::DriveInterruptInput(bool active) {
    if (active && !input_ && stored_written_) {
        emu_.Get<Fatal>().Die(
            "VRC5477 GIU interrupt input rose after guest writes with an unmodeled effect: "
            "0x00=0x%08X 0x08=0x%08X 0x10=0x%08X 0x18=0x%08X 0x20=0x%08X 0x28=0x%08X "
            "0x30=0x%08X 0x40=0x%08X",
            stored_[0], stored_[1], stored_[2], stored_[3], stored_[4], stored_[5], stored_[6],
            stored_[7]);
    }
    if (active && !input_) latched_ = true;
    input_ = active;
    DriveSource();
}

void Vrc5477Giu::DriveSource() {
    if (latched_ == line_) return;
    line_ = latched_;
    if (line_) intc_->AssertSource(kIntcSourceId);
    else       intc_->DeassertSource(kIntcSourceId);
}

void Vrc5477Giu::SaveState(StateWriter& w) {
    w.Write("giu_input", input_);
    w.Write("giu_latched", latched_);
    w.Write("giu_line", line_);
    w.WriteBytes("giu_stored", stored_, sizeof(stored_));
    w.Write("giu_stored_written", stored_written_);
}

void Vrc5477Giu::RestoreState(StateReader& r) {
    r.Read("giu_input", input_);
    r.Read("giu_latched", latched_);
    r.Read("giu_line", line_);
    r.ReadBytes("giu_stored", stored_, sizeof(stored_));
    r.Read("giu_stored_written", stored_written_);
}

void Vrc5477Giu::PostRestore() {
    if (line_) intc_->AssertSource(kIntcSourceId);
    else       intc_->DeassertSource(kIntcSourceId);
}

}

#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "../../cpu/vr5500/vr5500_id.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "vrc5477_intc.h"
#include "vrc5477_pci.h"
#include "vrc5477_timers.h"

#include <cstdint>
#include <cstring>

namespace cerfcore {

namespace {

constexpr uint32_t kBlockBase = 0x1FA00000u;
constexpr uint32_t kBlockSize = 0x800u;       /* low system-controller register block */

constexpr uint32_t kIntcLo = 0x400u;          /* INTC sub-block, delegated to Vrc5477Intc */
constexpr uint32_t kIntcHi = 0x490u;

constexpr uint32_t kPci0AliasFirst = 0x0E0u;
constexpr uint32_t kPci0AliasEnd   = 0x100u;
constexpr uint32_t kPci0AliasShift = 0x200u;

uint32_t CanonicalOffset(uint32_t off) {
    return off >= kPci0AliasFirst && off < kPci0AliasEnd ? off + kPci0AliasShift : off;
}

bool IsActiveBlock(uint32_t off, const char** which) {
    if (off >= 0x300u && off < 0x3C0u) { *which = "DMA"; return true; }
    return false;
}

class Vrc5477SysCon : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetSocId() == SocId::Vr5500;
    }
    void OnReady() override {
        timers_ = &emu_.Get<Vrc5477Timers>();
        pci_    = &emu_.Get<Vrc5477Pci>();
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            std::memset(regs_, 0, sizeof(regs_));
        });
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return kBlockBase; }
    uint32_t MmioSize() const override { return kBlockSize; }

    uint8_t  ReadByte (uint32_t addr) override            { return Read<uint8_t>(addr); }
    uint16_t ReadHalf (uint32_t addr) override            { return Read<uint16_t>(addr); }
    uint32_t ReadWord (uint32_t addr) override            { return Read<uint32_t>(addr); }
    void WriteByte(uint32_t addr, uint8_t  v) override    { Write<uint8_t>(addr, v); }
    void WriteHalf(uint32_t addr, uint16_t v) override    { Write<uint16_t>(addr, v); }
    void WriteWord(uint32_t addr, uint32_t v) override    { Write<uint32_t>(addr, v); }

    void SaveState(StateWriter& w) override {
        w.WriteBytes("regs", regs_, sizeof(regs_));
        emu_.Get<Vrc5477Intc>().SaveState(w);
        timers_->SaveState(w);
        pci_->SaveTargetState(w);
    }
    void RestoreState(StateReader& r) override {
        r.ReadBytes("regs", regs_, sizeof(regs_));
        emu_.Get<Vrc5477Intc>().RestoreState(r);
        timers_->RestoreState(r);
        pci_->RestoreTargetState(r);
    }
    void PostRestore() override { emu_.Get<Vrc5477Intc>().Renotify(); }

private:
    template <typename T> T Read(uint32_t addr) {
        const uint32_t off = CanonicalOffset(addr - kBlockBase);
        if (off >= kIntcLo && off < kIntcHi) {
            if constexpr (sizeof(T) == 4) {
                return static_cast<T>(emu_.Get<Vrc5477Intc>().ReadReg(off - kIntcLo));
            } else {
                HaltUnsupportedAccess("INTC sub-word read", addr, 0);
            }
        }
        if (Vrc5477Pci::Owns(off)) {
            if constexpr (sizeof(T) == 4) {
                return static_cast<T>(pci_->ReadReg(off));
            } else {
                return static_cast<T>(pci_->ReadRegPartial(off, sizeof(T)));
            }
        }
        if (Vrc5477Timers::Owns(off)) {
            if constexpr (sizeof(T) == 4) {
                return static_cast<T>(timers_->ReadReg(off));
            } else {
                HaltUnsupportedAccess("timer sub-word read", addr, 0);
            }
        }
        const char* which = nullptr;
        if (IsActiveBlock(off, &which)) {
            emu_.Get<Fatal>().Die("Vrc5477SysCon: %s read off=0x%03X not implemented "
                                  "(must be a full peripheral)", which, off);
        }
        T v{};
        std::memcpy(&v, &regs_[off], sizeof(T));
        return v;
    }
    template <typename T> void Write(uint32_t addr, T v) {
        const uint32_t off = CanonicalOffset(addr - kBlockBase);
        if (off >= kIntcLo && off < kIntcHi) {
            if constexpr (sizeof(T) == 4) {
                emu_.Get<Vrc5477Intc>().WriteReg(off - kIntcLo, static_cast<uint32_t>(v));
                return;
            } else {
                HaltUnsupportedAccess("INTC sub-word write", addr, v);
            }
        }
        if (Vrc5477Pci::Owns(off)) {
            if constexpr (sizeof(T) == 4) {
                pci_->WriteReg(off, static_cast<uint32_t>(v));
            } else {
                pci_->WriteRegPartial(off, static_cast<uint32_t>(v), sizeof(T));
            }
            return;
        }
        if (Vrc5477Pci::IsBusControlHigh(off)) {
            if constexpr (sizeof(T) == 4) {
                std::memcpy(&regs_[off], &v, sizeof(T));
                pci_->BusControlHighWritten(off, static_cast<uint32_t>(v));
                return;
            } else {
                HaltUnsupportedAccess("PCI control-high sub-word write", addr, v);
            }
        }
        if (Vrc5477Timers::Owns(off)) {
            if constexpr (sizeof(T) == 4) {
                timers_->WriteReg(off, static_cast<uint32_t>(v));
                return;
            } else {
                HaltUnsupportedAccess("timer sub-word write", addr, v);
            }
        }
        const char* which = nullptr;
        if (IsActiveBlock(off, &which)) {
            emu_.Get<Fatal>().Die("Vrc5477SysCon: %s write off=0x%03X val=0x%08X not "
                                  "implemented (must be a full peripheral)", which, off,
                                  static_cast<uint32_t>(v));
        }
        std::memcpy(&regs_[off], &v, sizeof(T));
    }

    Vrc5477Timers* timers_ = nullptr;
    Vrc5477Pci*    pci_    = nullptr;
    uint8_t        regs_[kBlockSize] = {};
};

}  /* namespace */

REGISTER_SERVICE(Vrc5477SysCon);

}

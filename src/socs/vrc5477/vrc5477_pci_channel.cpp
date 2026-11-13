#include "vrc5477_pci_channel.h"

#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/pci/pci_device.h"

#include <bit>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kPdarAddrMask         = 0xFFE00000u;
constexpr uint32_t kPdarMaskBits         = 0x0000000Fu;
constexpr uint32_t kPdarOnMemoryBus      = 0x00000010u;
constexpr uint32_t kPdarPciVisible       = 0x00000020u;
constexpr uint32_t kPdarWidthShift       = 6u;
constexpr uint32_t kPdarWidthMask        = 0x3u;
constexpr uint32_t kPdarWidth32          = 2u;
constexpr uint32_t kPdarLowBits          = 0x000000FFu;
constexpr uint32_t kPdarSmallestMaskBits = 5u;
constexpr uint32_t kPdarLargestMaskBits  = 15u;
constexpr uint32_t kPdarSizeShiftBase    = 36u;

constexpr uint32_t kPmrAddrMask  = 0xFFE00000u;
constexpr uint32_t kPmrTypeMask  = 0x0000000Eu;
constexpr uint32_t kPmrTypeIo    = 0x00000002u;
constexpr uint32_t kPmrTypeMem   = 0x00000006u;
constexpr uint32_t kPmrTypeCfg   = 0x0000000Au;
constexpr uint32_t kPmrAccess32  = 0x00000010u;
constexpr uint32_t kPmrCfgType1  = 0x00000200u;
constexpr uint32_t kPmrKnownBits = kPmrAddrMask | kPmrTypeMask | kPmrAccess32 | kPmrCfgType1;

constexpr uint32_t kConfigIdselShift = 11u;
constexpr uint32_t kConfigFnShift    = 8u;
constexpr uint32_t kConfigFnMask     = 0x7u;
constexpr uint32_t kConfigRegMask    = 0xFFu;
constexpr uint32_t kDwordMask        = ~3u;

constexpr const char* kRegisterNames[] = {"window 0", "initiator 0", "window 1", "initiator 1"};

uint32_t Extract(uint32_t dword, uint32_t reg, unsigned size) {
    return (dword >> ((reg & 3u) * 8u)) & cerf::ByteWidthMask(size);
}

uint32_t Merge(uint32_t dword, uint32_t value, uint32_t reg, unsigned size) {
    const unsigned shift = (reg & 3u) * 8u;
    const uint32_t mask  = cerf::ByteWidthMask(size) << shift;
    return (dword & ~mask) | ((value << shift) & mask);
}

}

Vrc5477PciChannel::Vrc5477PciChannel(CerfEmulator& emu, const char* name, uint32_t window_base,
                                     uint32_t window_size, uint32_t io_window_base, uint32_t io_window_size)
    : emu_(emu),
      name_(name),
      window_base_(window_base),
      window_size_(window_size),
      io_window_base_(io_window_base),
      io_window_size_(io_window_size) {}

void Vrc5477PciChannel::RegisterDevice(PciDevice* dev) { devices_.push_back(dev); }

void Vrc5477PciChannel::RegisterBusResetListener(std::function<void()> listener) {
    bus_reset_.Add(std::move(listener));
}

uint32_t Vrc5477PciChannel::ReadRegister(Register reg) const { return regs_[static_cast<int>(reg)]; }

void Vrc5477PciChannel::WriteRegister(Register reg, uint32_t value) { regs_[static_cast<int>(reg)] = value; }

void Vrc5477PciChannel::WriteUpperWord(Register reg, uint32_t value) {
    if (value != 0u) {
        emu_.Get<Fatal>().Die("VRC5477 %s PCI: upper word of %s = 0x%08X", name_,
                              kRegisterNames[static_cast<int>(reg)], value);
    }
}

void Vrc5477PciChannel::ResetControl() {
    for (uint32_t& r : regs_) r = 0u;
    reset_held_ = false;
}

void Vrc5477PciChannel::SetResetLine(bool asserted) {
    if (asserted && !reset_held_) bus_reset_.Call();
    reset_held_ = asserted;
}

void Vrc5477PciChannel::Reject(const char* what, uint32_t addr, uint32_t value) {
    emu_.Get<Fatal>().Die("VRC5477 %s PCI: %s addr=0x%08X val=0x%08X window0=0x%08X initiator0=0x%08X "
                          "window1=0x%08X initiator1=0x%08X reset=%d",
                          name_, what, addr, value, regs_[0], regs_[1], regs_[2], regs_[3], reset_held_ ? 1 : 0);
}

uint32_t Vrc5477PciChannel::WindowOffset(uint32_t pdar, uint32_t pa, uint32_t* size) {
    if (reset_held_) Reject("access while the bus is held in reset", pa, 0u);
    const uint32_t mask_bits = pdar & kPdarMaskBits;
    const uint32_t width     = (pdar >> kPdarWidthShift) & kPdarWidthMask;
    if (mask_bits < kPdarSmallestMaskBits || mask_bits > kPdarLargestMaskBits || width != kPdarWidth32 ||
        (pdar & (kPdarOnMemoryBus | kPdarPciVisible)) != 0u || (pdar & ~(kPdarAddrMask | kPdarLowBits)) != 0u) {
        Reject("access with a window register setting that is not modeled", pa, 0u);
    }
    *size = 1u << (kPdarSizeShiftBase - mask_bits);
    const uint32_t base = pdar & kPdarAddrMask;
    if (pa < base || pa - base >= *size) Reject("access outside the programmed window", pa, 0u);
    return pa - base;
}

uint32_t Vrc5477PciChannel::PciAddress(uint32_t pa, bool wide, Cycle* cycle) {
    uint32_t       size   = 0;
    const uint32_t offset = WindowOffset(regs_[0], pa, &size);
    const uint32_t pmr    = regs_[1];
    if ((pmr & ~kPmrKnownBits) != 0u) Reject("cycle with an initiator register setting that is not modeled", pa, 0u);
    if (wide && (pmr & kPmrAccess32) == 0u) Reject("64-bit access without the 32-bit access bit", pa, 0u);
    const uint32_t type  = pmr & kPmrTypeMask;
    const bool     type1 = (pmr & kPmrCfgType1) != 0u;
    if (type == kPmrTypeMem && !type1)      *cycle = Cycle::kMemory;
    else if (type == kPmrTypeCfg && !type1) *cycle = Cycle::kConfigType0;
    else if (type == kPmrTypeCfg)           *cycle = Cycle::kConfigType1;
    else Reject("cycle type that is not modeled", pa, 0u);
    const uint32_t pci_base = pmr & kPmrAddrMask;
    if ((pci_base & (size - 1u)) != 0u) Reject("initiator address inside the window", pa, 0u);
    return pci_base | offset;
}

uint32_t Vrc5477PciChannel::PciIoAddress(uint32_t pa) {
    uint32_t       size   = 0;
    const uint32_t offset = WindowOffset(regs_[2], pa, &size);
    const uint32_t pmr    = regs_[3];
    if ((pmr & ~kPmrKnownBits) != 0u || (pmr & kPmrCfgType1) != 0u || (pmr & kPmrTypeMask) != kPmrTypeIo) {
        Reject("I/O window cycle with an initiator register setting that is not modeled", pa, 0u);
    }
    const uint32_t pci_base = pmr & kPmrAddrMask;
    if ((pci_base & (size - 1u)) != 0u) Reject("I/O initiator address inside the window", pa, 0u);
    return pci_base | offset;
}

PciDevice* Vrc5477PciChannel::ConfigDevice(uint32_t pci, uint32_t* reg) {
    const uint32_t idsel = pci >> kConfigIdselShift;
    if (idsel == 0u) return nullptr;
    if ((idsel & (idsel - 1u)) != 0u) Reject("configuration cycle on more than one IDSEL line", pci, 0u);
    const uint8_t  dev = DeviceOfIdselLine(kFirstIdselLine + static_cast<uint32_t>(std::countr_zero(idsel)));
    const uint32_t fnc = (pci >> kConfigFnShift) & kConfigFnMask;
    for (PciDevice* d : devices_) {
        if (d->PciDev() == dev && d->PciFnc() == fnc) {
            *reg = pci & kConfigRegMask;
            return d;
        }
    }
    return nullptr;
}

uint64_t Vrc5477PciChannel::ReadDword(uint32_t pa) {
    Cycle cycle = Cycle::kMemory;
    PciAddress(pa, true, &cycle);
    if (cycle != Cycle::kMemory) Reject("64-bit configuration read", pa, 0u);
    return Read(pa, 4) | (static_cast<uint64_t>(Read(pa + 4u, 4)) << 32);
}

void Vrc5477PciChannel::WriteDword(uint32_t pa, uint64_t value) {
    Cycle cycle = Cycle::kMemory;
    PciAddress(pa, true, &cycle);
    if (cycle != Cycle::kMemory) Reject("64-bit configuration write", pa, static_cast<uint32_t>(value));
    Write(pa, static_cast<uint32_t>(value), 4);
    Write(pa + 4u, static_cast<uint32_t>(value >> 32), 4);
}

uint32_t Vrc5477PciChannel::Read(uint32_t pa, unsigned size) {
    Cycle          cycle = Cycle::kMemory;
    const uint32_t pci   = PciAddress(pa, false, &cycle);
    if (cycle == Cycle::kConfigType1) return cerf::ByteWidthMask(size);
    if (cycle == Cycle::kConfigType0) {
        uint32_t   reg = 0;
        PciDevice* d   = ConfigDevice(pci, &reg);
        if (!d) return cerf::ByteWidthMask(size);
        return Extract(d->ConfigRead(reg & kDwordMask), reg, size);
    }
    for (PciDevice* d : devices_)
        if (d->MemClaims(pci)) return d->MemRead(pci, size);
    return cerf::ByteWidthMask(size);
}

void Vrc5477PciChannel::Write(uint32_t pa, uint32_t value, unsigned size) {
    Cycle          cycle = Cycle::kMemory;
    const uint32_t pci   = PciAddress(pa, false, &cycle);
    if (cycle == Cycle::kConfigType1) return;
    if (cycle == Cycle::kConfigType0) {
        uint32_t   reg = 0;
        PciDevice* d   = ConfigDevice(pci, &reg);
        if (!d) return;
        if (size >= 4u) {
            d->ConfigWrite(reg & kDwordMask, value);
            return;
        }
        d->ConfigWrite(reg & kDwordMask, Merge(d->ConfigRead(reg & kDwordMask), value, reg, size));
        return;
    }
    for (PciDevice* d : devices_) {
        if (d->MemClaims(pci)) {
            d->MemWrite(pci, value, size);
            return;
        }
    }
}

uint32_t Vrc5477PciChannel::IoRead(uint32_t pa, unsigned size) {
    const uint32_t pci_io = PciIoAddress(pa);
    for (PciDevice* d : devices_)
        if (d->IoClaims(pci_io)) return d->IoRead(pci_io, size);
    return cerf::ByteWidthMask(size);
}

void Vrc5477PciChannel::IoWrite(uint32_t pa, uint32_t value, unsigned size) {
    const uint32_t pci_io = PciIoAddress(pa);
    for (PciDevice* d : devices_) {
        if (d->IoClaims(pci_io)) {
            d->IoWrite(pci_io, value, size);
            return;
        }
    }
}

void Vrc5477PciChannel::SaveState(StateWriter& w) {
    w.WriteBytes("pci_control", regs_, sizeof(regs_));
    w.Write("pci_reset_held", reset_held_);
    w.Write<uint32_t>("devices_count", static_cast<uint32_t>(devices_.size()));
    for (PciDevice* d : devices_) {
        w.BeginFrame(static_cast<uint32_t>((d->PciDev() << 8) | d->PciFnc()));
        d->SaveState(w);
        w.EndFrame();
    }
}

void Vrc5477PciChannel::RestoreState(StateReader& r) {
    r.ReadBytes("pci_control", regs_, sizeof(regs_));
    r.Read("pci_reset_held", reset_held_);
    uint32_t n = 0;
    r.Read("devices_count", n);
    if (n != devices_.size())
        r.Reject("VRC5477 %s PCI: %u devices, this build has %zu", name_, n, devices_.size());
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t tag   = r.EnterFrame();
        const uint8_t  dev   = static_cast<uint8_t>(tag >> 8);
        const uint8_t  fnc   = static_cast<uint8_t>(tag & 0xFFu);
        size_t         match = devices_.size();
        for (size_t k = 0; k < devices_.size(); ++k) {
            if (devices_[k]->PciDev() == dev && devices_[k]->PciFnc() == fnc) {
                match = k;
                break;
            }
        }
        if (match == devices_.size())
            r.Reject("VRC5477 %s PCI: no device dev=%u fnc=%u in this build", name_, dev, fnc);
        devices_[match]->RestoreState(r);
        r.LeaveFrame();
    }
}

void Vrc5477PciChannel::PostRestore() {
    for (PciDevice* d : devices_) d->PostRestore();
}

}

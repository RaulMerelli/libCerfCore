#include "ali_m5229_ide.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

#include <cstring>

namespace cerfcore {

namespace {

constexpr uint8_t  kRevision  = 0xC3u;
constexpr uint32_t kClassBase = 0x01010000u;

constexpr PciHeaderIdentity kHeaderIdentity{
    {0x522910B9u, 0u, 0x0290u, 0x0000u, 0x0145u, 0x00u, 0xFFu, 0x00u}, 0x00u, 0xFFu, 0x04020100u};
constexpr PciPowerManagement kPowerManagement{0x60u, 0x00020001u};

constexpr uint8_t kProgIfReset      = 0xFAu;
constexpr uint8_t kProgIfFixed      = 0x8Au;
constexpr uint8_t kProgIfStatusBits = 0x70u;
constexpr uint8_t kProgIfPrimary    = 0x20u;
constexpr uint8_t kProgIfSecondary  = 0x10u;
constexpr uint8_t kProgIfNative     = 0x05u;

constexpr uint32_t kRegClassAttr1  = 0x43u;
constexpr uint32_t kRegUdmaTest    = 0x4Bu;
constexpr uint32_t kRegClassAttr2  = 0x4Du;
constexpr uint32_t kRegReserved4E  = 0x4Eu;
constexpr uint32_t kRegReserved4F  = 0x4Fu;
constexpr uint32_t kRegClassAttr3  = 0x50u;
constexpr uint32_t kRegResetTest   = 0x51u;
constexpr uint32_t kRegChannelSet  = 0x52u;
constexpr uint32_t kRegCdromCtl    = 0x53u;
constexpr uint32_t kRegTimingFirst = 0x54u;
constexpr uint32_t kRegTimingLast  = 0x5Fu;
constexpr uint32_t kRegIdeClock    = 0x78u;

constexpr uint8_t kAttr2StatusLock = 0x80u;
constexpr uint8_t kAttr3Reserved   = 0xD4u;
constexpr uint8_t kAttr3StatusRead = 0x02u;
constexpr uint8_t kAttr3Enable     = 0x01u;
constexpr uint8_t kChannelRemap    = 0xF0u;
constexpr uint8_t kCdromReserved   = 0x74u;
constexpr uint8_t kCdromSubsysLock = 0x80u;
constexpr uint8_t kCdromMaskBars   = 0x08u;

constexpr uint32_t kPrimaryCmd   = 0x1F0u;
constexpr uint32_t kSecondaryCmd = 0x170u;
constexpr uint32_t kCmdSize      = 8u;
constexpr uint32_t kPrimaryCtl   = 0x3F6u;
constexpr uint32_t kSecondaryCtl = 0x376u;

constexpr size_t kBusMasterBar  = 4u;
constexpr size_t kChannelBars   = 4u;

constexpr PciBar kBarReset[] = {
    {~(8u - 1u), PciBar::kIoSpace, 0x1F0u},
    {~(4u - 1u), PciBar::kIoSpace, 0x3F4u},
    {~(8u - 1u), PciBar::kIoSpace, 0x170u},
    {~(4u - 1u), PciBar::kIoSpace, 0x374u},
    {~(16u - 1u), PciBar::kIoSpace, 0xF000u},
};

}

AliM5229Ide::AliM5229Ide(CerfEmulator& emu, const AliM1535BridgeConfig& bridge, uint32_t idsel_line)
    : emu_(emu), bridge_(bridge), idsel_line_(idsel_line), header_(kHeaderIdentity) {
    for (size_t i = 0; i < sizeof(kBarReset) / sizeof(kBarReset[0]); ++i) header_.SetBarReset(i, kBarReset[i]);
    Reset();
}

void AliM5229Ide::Reset() {
    header_.Reset();
    prog_if_          = kProgIfReset;
    subsystem_vendor_ = 0u;
    subsystem_device_ = 0u;
    pm_               = kPowerManagement;
    std::memset(dev_regs_, 0, sizeof(dev_regs_));
    dev_regs_[kRegUdmaTest - kDeviceRegsFirst]   = 0x4Au;
    dev_regs_[kRegReserved4E - kDeviceRegsFirst] = 0xBAu;
    dev_regs_[kRegReserved4F - kDeviceRegsFirst] = 0x1Au;
    dev_regs_[kRegClassAttr3 - kDeviceRegsFirst] = kAttr3StatusRead;
    dev_regs_[0x54u - kDeviceRegsFirst] = 0x55u;
    dev_regs_[0x55u - kDeviceRegsFirst] = 0x55u;
    dev_regs_[0x56u - kDeviceRegsFirst] = 0x44u;
    dev_regs_[0x57u - kDeviceRegsFirst] = 0x44u;
    dev_regs_[kRegIdeClock - kDeviceRegsFirst] = 0x21u;
}

void AliM5229Ide::Reject(const char* what, uint32_t addr, uint32_t value) {
    emu_.Get<Fatal>().Die("ALi M5229 IDE AD%u: %s addr=0x%08X val=0x%08X command=0x%04X progif=0x%02X", idsel_line_,
                          what, addr, value, header_.Command(), prog_if_);
}

bool AliM5229Ide::Compatibility() const { return (prog_if_ & kProgIfNative) == 0u; }

uint8_t AliM5229Ide::ProgIfRead() const {
    if ((Dev(kRegClassAttr3) & kAttr3StatusRead) == 0u) return static_cast<uint8_t>(prog_if_ & ~kProgIfStatusBits);
    return prog_if_;
}

void AliM5229Ide::WriteProgIf(uint8_t value) {
    uint8_t writable = static_cast<uint8_t>(~kProgIfFixed & ~Dev(kRegClassAttr1));
    if ((Dev(kRegClassAttr2) & kAttr2StatusLock) != 0u) writable &= static_cast<uint8_t>(~kProgIfStatusBits);
    const uint8_t next = static_cast<uint8_t>((prog_if_ & ~writable) | (value & writable));
    if ((next & kProgIfNative) != 0u) Reject("native-mode programming interface", PciCommonHeader::kRegClass, value);
    prog_if_ = next;
}

uint8_t AliM5229Ide::DeviceByteRead(uint32_t reg) const {
    if (reg == kRegClassAttr3 && bridge_.Visible(AliM1535Function::kIde))
        return static_cast<uint8_t>(Dev(reg) | kAttr3Enable);
    return Dev(reg);
}

uint32_t AliM5229Ide::ConfigRead(uint32_t reg) {
    if (reg == PciCommonHeader::kRegClass)
        return kClassBase | (static_cast<uint32_t>(ProgIfRead()) << 8) | kRevision;
    const bool channel_bar = reg >= PciType0Header::kRegBarFirst && reg < PciType0Header::kRegBarFirst + 4u * kChannelBars;
    if (channel_bar && Compatibility() && (Dev(kRegCdromCtl) & kCdromMaskBars) != 0u) return 0u;
    uint32_t value = 0u;
    if (header_.Read(reg, &value)) return value;
    if (reg == PciType0Header::kRegSubsystem)
        return subsystem_vendor_ | (static_cast<uint32_t>(subsystem_device_) << 16);
    if (reg == PciType0Header::kRegCapPtr) return pm_.offset;
    if (pm_.Owns(reg)) return pm_.Read(reg);
    if (reg < kDeviceRegsFirst || reg >= kDeviceRegsFirst + kDeviceRegsCount) return 0u;
    return static_cast<uint32_t>(DeviceByteRead(reg)) | (static_cast<uint32_t>(DeviceByteRead(reg + 1u)) << 8) |
           (static_cast<uint32_t>(DeviceByteRead(reg + 2u)) << 16) |
           (static_cast<uint32_t>(DeviceByteRead(reg + 3u)) << 24);
}

void AliM5229Ide::ConfigWrite(uint32_t reg, uint32_t value) {
    if (reg == PciCommonHeader::kRegClass) {
        WriteProgIf(static_cast<uint8_t>(value >> 8));
    } else if (header_.Write(reg, value)) {
        return;
    } else if (reg == PciType0Header::kRegSubsystem) {
        if ((Dev(kRegCdromCtl) & kCdromSubsysLock) == 0u) {
            subsystem_vendor_ = static_cast<uint16_t>(value);
            subsystem_device_ = static_cast<uint16_t>(value >> 16);
        }
    } else if (pm_.Owns(reg)) {
        if (const char* why = pm_.WriteHoldingD0(reg, value)) Reject(why, reg, value);
    } else if (reg >= kDeviceRegsFirst && reg < kDeviceRegsFirst + kDeviceRegsCount) {
        for (uint32_t i = 0; i < 4u; ++i) WriteDeviceByte(reg + i, static_cast<uint8_t>(value >> (i * 8u)));
    }
}

void AliM5229Ide::WriteDeviceByte(uint32_t reg, uint8_t value) {
    uint8_t& slot = dev_regs_[reg - kDeviceRegsFirst];
    switch (reg) {
        case kRegClassAttr1:
        case kRegUdmaTest:
        case kRegClassAttr2:
        case kRegIdeClock:
            slot = value;
            return;
        case kRegClassAttr3:
            if ((value & kAttr3Reserved) != 0u || (value & kAttr3Enable) == 0u)
                Reject("class code attribute III setting that is not modeled", reg, value);
            slot = static_cast<uint8_t>(value & ~kAttr3Enable);
            return;
        case kRegResetTest:
            if (value != 0u) Reject("reset or test mode", reg, value);
            return;
        case kRegChannelSet:
            if ((value & kChannelRemap) != 0u) Reject("channel remapping", reg, value);
            slot = value;
            return;
        case kRegCdromCtl:
            if ((value & kCdromReserved) != 0u) Reject("CD-ROM control reserved bits", reg, value);
            slot = value;
            return;
        default:
            if (reg >= kRegTimingFirst && reg <= kRegTimingLast) slot = value;
            return;
    }
}

bool AliM5229Ide::IoClaims(uint32_t pci_io) const {
    if (!bridge_.Visible(AliM1535Function::kIde) || (header_.Command() & PciCommonHeader::kCommandIo) == 0u)
        return false;
    if ((prog_if_ & kProgIfPrimary) != 0u && (pci_io - kPrimaryCmd < kCmdSize || pci_io == kPrimaryCtl)) return true;
    if ((prog_if_ & kProgIfSecondary) != 0u && (pci_io - kSecondaryCmd < kCmdSize || pci_io == kSecondaryCtl))
        return true;
    return header_.Bar(kBusMasterBar).Decodes(pci_io);
}

uint32_t AliM5229Ide::IoRead(uint32_t pci_io, unsigned size) { Reject("ATA or bus master register read", pci_io, size); }

void AliM5229Ide::IoWrite(uint32_t pci_io, uint32_t value, unsigned) {
    Reject("ATA or bus master register write", pci_io, value);
}

uint32_t AliM5229Ide::MemRead(uint32_t pci_addr, unsigned) { Reject("memory read", pci_addr, 0u); }

void AliM5229Ide::MemWrite(uint32_t pci_addr, uint32_t value, unsigned) { Reject("memory write", pci_addr, value); }

void AliM5229Ide::SaveState(StateWriter& w) {
    w.Write("ide_prog_if", prog_if_);
    w.Write("ide_subsystem_vendor", subsystem_vendor_);
    w.Write("ide_subsystem_device", subsystem_device_);
    StateWriteField field(w);
    PciType0Header::Visit(header_, field);
    PciPowerManagement::Visit(pm_, field);
    w.WriteBytes("ide_dev_regs", dev_regs_, sizeof(dev_regs_));
}

void AliM5229Ide::RestoreState(StateReader& r) {
    r.Read("ide_prog_if", prog_if_);
    r.Read("ide_subsystem_vendor", subsystem_vendor_);
    r.Read("ide_subsystem_device", subsystem_device_);
    StateReadField field(r);
    PciType0Header::Visit(header_, field);
    PciPowerManagement::Visit(pm_, field);
    r.ReadBytes("ide_dev_regs", dev_regs_, sizeof(dev_regs_));
}

}

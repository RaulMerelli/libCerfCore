#include "vrc5477_pci.h"

#include <cerfcore/boards/board_context.h>
#include "../../boards/page_table_builder.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/fatal.h>
#include "../../cpu/physical_bus.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../cpu/vr5500/vr5500_id.h"
#include "vrc5477_pdar.h"

#include <utility>

namespace cerfcore {

namespace {

using Reg = Vrc5477PciChannel::Register;

constexpr uint32_t kExternalWindowBase   = 0x08000000u;
constexpr uint32_t kExternalWindowSize   = 0x08000000u;
constexpr uint32_t kExternalIoWindowBase = 0x14000000u;
constexpr uint32_t kExternalIoWindowSize = 0x02000000u;
constexpr uint32_t kInternalWindowBase   = 0x1D000000u;
constexpr uint32_t kInternalWindowSize   = 0x01000000u;
constexpr uint32_t kInternalIoWindowBase = 0x1C000000u;
constexpr uint32_t kInternalIoWindowSize = 0x01000000u;

constexpr uint32_t kOffPciCtl0H = 0x2E4u;
constexpr uint32_t kOffPciCtl1H = 0x6E4u;

constexpr uint32_t kPciCtlHighColdReset = 0x80000000u;
constexpr uint32_t kPciCtlHighWarmReset = 0x40000000u;

constexpr uint32_t kOffSdram0       = 0x000u;
constexpr uint32_t kOffSdram1       = 0x008u;
constexpr uint32_t kOffTargetCmd[2] = {0x204u, 0x604u};
constexpr uint32_t kOffSdramBar[2]  = {0x218u, 0x618u};

constexpr uint32_t kCommandMemory      = 0x0002u;
constexpr unsigned kTargetCommandBytes = 2u;

int TargetCommandChannel(uint32_t off) {
    for (int i = 0; i < 2; ++i)
        if ((off & ~3u) == kOffTargetCmd[i]) return i;
    return -1;
}

bool IsTargetReg(uint32_t off) {
    return off == kOffSdram0 || off == kOffSdram1 || TargetCommandChannel(off) >= 0 || off == kOffSdramBar[0] ||
           off == kOffSdramBar[1];
}

struct ControlRegister {
    uint32_t            off;
    Vrc5477PciChannelId channel;
    Reg                 reg;
    bool                upper;
};

constexpr ControlRegister kControlRegisters[] = {
    {0x060u, Vrc5477PciChannelId::kExternal, Reg::kWindow0, false},
    {0x064u, Vrc5477PciChannelId::kExternal, Reg::kWindow0, true},
    {0x068u, Vrc5477PciChannelId::kExternal, Reg::kWindow1, false},
    {0x06Cu, Vrc5477PciChannelId::kExternal, Reg::kWindow1, true},
    {0x0D0u, Vrc5477PciChannelId::kInternal, Reg::kWindow0, false},
    {0x0D4u, Vrc5477PciChannelId::kInternal, Reg::kWindow0, true},
    {0x0D8u, Vrc5477PciChannelId::kInternal, Reg::kWindow1, false},
    {0x0DCu, Vrc5477PciChannelId::kInternal, Reg::kWindow1, true},
    {0x2F0u, Vrc5477PciChannelId::kExternal, Reg::kInitiator0, false},
    {0x2F4u, Vrc5477PciChannelId::kExternal, Reg::kInitiator0, true},
    {0x2F8u, Vrc5477PciChannelId::kExternal, Reg::kInitiator1, false},
    {0x2FCu, Vrc5477PciChannelId::kExternal, Reg::kInitiator1, true},
    {0x6F0u, Vrc5477PciChannelId::kInternal, Reg::kInitiator0, false},
    {0x6F4u, Vrc5477PciChannelId::kInternal, Reg::kInitiator0, true},
    {0x6F8u, Vrc5477PciChannelId::kInternal, Reg::kInitiator1, false},
    {0x6FCu, Vrc5477PciChannelId::kInternal, Reg::kInitiator1, true},
};

const ControlRegister* FindControlRegister(uint32_t block_off) {
    for (const ControlRegister& c : kControlRegisters)
        if (c.off == block_off) return &c;
    return nullptr;
}

constexpr uint8_t kAc97Dev        = Vrc5477PciChannel::DeviceOfIdselLine(29u);
constexpr uint8_t kUsbFunctionDev = Vrc5477PciChannel::DeviceOfIdselLine(30u);
constexpr uint8_t kUsbHostDev     = Vrc5477PciChannel::DeviceOfIdselLine(31u);

constexpr uint32_t kAc97IoSize  = 0x80u;
constexpr uint32_t kOhciMemSize = 0x1000u;

constexpr uint32_t kNecVendor    = 0x00001033u;
constexpr uint32_t kAc97Identity = 0x00A61033u;
constexpr uint32_t kOhciClass    = 0x0C031000u;

}

REGISTER_SERVICE_AS(Vrc5477Pci, PciHostBridge);

Vrc5477Pci::Vrc5477Pci(CerfEmulator& emu)
    : PciHostBridge(emu),
      external_(emu, "external", kExternalWindowBase, kExternalWindowSize, kExternalIoWindowBase,
                kExternalIoWindowSize),
      internal_(emu, "internal", kInternalWindowBase, kInternalWindowSize, kInternalIoWindowBase,
                kInternalIoWindowSize),
      ac97_(emu, kAc97Dev, kAc97Identity, 0u, PciBar{~(kAc97IoSize - 1u), PciBar::kIoSpace, 0u}),
      usb_function_(emu, kUsbFunctionDev, kNecVendor, 0u, PciBar{}),
      usb_host_(emu, kUsbHostDev, kNecVendor, kOhciClass, PciBar{~(kOhciMemSize - 1u), 0u, 0u}) {}

bool Vrc5477Pci::ShouldRegister() {
    return emu_.Get<BoardContext>().GetSocId() == SocId::Vr5500;
}

void Vrc5477Pci::OnReady() {
    internal_.RegisterDevice(&ac97_);
    internal_.RegisterDevice(&usb_function_);
    internal_.RegisterDevice(&usb_host_);
    internal_.RegisterBusResetListener([this] { ResetInternalFunctions(); });
    ResetInternalFunctions();
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
        external_.ResetControl();
        internal_.ResetControl();
        ResetInternalFunctions();
        ResetTarget();
    });
}

void Vrc5477Pci::ResetTarget() {
    for (int i = 0; i < 2; ++i) {
        sdram_bank_[i]     = 0u;
        target_command_[i] = 0u;
        sdram_bar_[i]      = 0u;
    }
}

bool Vrc5477Pci::WriteTargetReg(uint32_t block_off, uint32_t value) {
    if (block_off == kOffSdram0) {
        const Vrc5477Pdar pdar{value};
        if (!pdar.Enabled() || pdar.OnMemoryBus() || !pdar.Width32() || pdar.StrayBits())
            emu_.Get<Fatal>().Die("VRC5477: SDRAM bank 0 window setting that is not modeled val=0x%08X", value);
        if (!BacksSdram(pdar.Base(), pdar.Size()))
            emu_.Get<Fatal>().Die("VRC5477: SDRAM bank 0 base 0x%08X size 0x%08X is not the emulated DRAM",
                                  pdar.Base(), pdar.Size());
        sdram_bank_[0] = value;
        return true;
    }
    if (block_off == kOffSdram1) {
        if (Vrc5477Pdar{value}.MaskBits() != 0u)
            emu_.Get<Fatal>().Die("VRC5477: SDRAM bank 1 enabled val=0x%08X", value);
        sdram_bank_[1] = value;
        return true;
    }
    for (int i = 0; i < 2; ++i) {
        if (block_off == kOffTargetCmd[i]) {
            target_command_[i] = value & 0xFFFFu;
            return true;
        }
        if (block_off == kOffSdramBar[i]) {
            sdram_bar_[i] = value;
            return true;
        }
    }
    return false;
}

bool Vrc5477Pci::BacksSdram(uint32_t base, uint32_t size) const {
    for (const BackedRegion& r : emu_.Get<PageTableBuilder>().BackedMemoryRegions())
        if (r.kind == MemoryKind::kRam && r.pa_base == base && r.size == size) return true;
    return false;
}

uint32_t Vrc5477Pci::BusMasterPa(uint32_t pci_addr) {
    const Vrc5477Pdar pdar{sdram_bank_[0]};
    if ((target_command_[0] & kCommandMemory) == 0u || !pdar.PciVisible() || !pdar.Enabled()) {
        emu_.Get<Fatal>().Die("VRC5477 external PCI: bus master cycle with the SDRAM target disabled pci=0x%08X "
                              "command=0x%04X sdram0=0x%08X",
                              pci_addr, target_command_[0], pdar.value);
    }
    const uint32_t size   = pdar.Size();
    const uint32_t base   = sdram_bar_[0] & ~(size - 1u);
    const uint32_t offset = pci_addr - base;
    if (offset >= size || (pci_addr & 3u) != 0u) {
        emu_.Get<Fatal>().Die("VRC5477 external PCI: bus master cycle outside the SDRAM window pci=0x%08X bar=0x%08X",
                              pci_addr, sdram_bar_[0]);
    }
    return pdar.Base() + offset;
}

uint32_t Vrc5477Pci::ReadRegPartial(uint32_t block_off, unsigned size) {
    const int ch = TargetCommandChannel(block_off);
    if (ch < 0) emu_.Get<Fatal>().Die("VRC5477 PCI: control sub-word read off=0x%03X size=%u", block_off, size);
    if ((block_off & 3u) + size > kTargetCommandBytes)
        emu_.Get<Fatal>().Die("VRC5477 PCI: target status read off=0x%03X size=%u", block_off, size);
    return (target_command_[ch] >> ((block_off & 3u) * 8u)) & cerf::ByteWidthMask(size);
}

void Vrc5477Pci::WriteRegPartial(uint32_t block_off, uint32_t value, unsigned size) {
    const int ch = TargetCommandChannel(block_off);
    if (ch < 0) {
        emu_.Get<Fatal>().Die("VRC5477 PCI: control sub-word write off=0x%03X val=0x%08X size=%u", block_off, value,
                              size);
    }
    const uint32_t shift = (block_off & 3u) * 8u;
    const uint32_t lanes = (cerf::ByteWidthMask(size) << shift) & 0xFFFFu;
    target_command_[ch]  = (target_command_[ch] & ~lanes) | ((value << shift) & lanes);
}

uint32_t Vrc5477Pci::BusMasterRead32(uint32_t pci_addr) {
    const uint32_t pa    = BusMasterPa(pci_addr);
    uint32_t       value = 0u;
    if (!emu_.Get<PhysicalBus>().Read(pa, MmioWidth::kWord, &value))
        emu_.Get<Fatal>().Die("VRC5477 external PCI: bus master read of unbacked SDRAM pa=0x%08X", pa);
    return value;
}

void Vrc5477Pci::BusMasterWrite32(uint32_t pci_addr, uint32_t value) {
    const uint32_t pa = BusMasterPa(pci_addr);
    if (!emu_.Get<PhysicalBus>().Write(pa, MmioWidth::kWord, value))
        emu_.Get<Fatal>().Die("VRC5477 external PCI: bus master write of unbacked SDRAM pa=0x%08X", pa);
}

void Vrc5477Pci::SaveTargetState(StateWriter& w) {
    w.WriteBytes("pci_sdram_bank", sdram_bank_, sizeof(sdram_bank_));
    w.WriteBytes("pci_target_command", target_command_, sizeof(target_command_));
    w.WriteBytes("pci_sdram_bar", sdram_bar_, sizeof(sdram_bar_));
}

void Vrc5477Pci::RestoreTargetState(StateReader& r) {
    r.ReadBytes("pci_sdram_bank", sdram_bank_, sizeof(sdram_bank_));
    r.ReadBytes("pci_target_command", target_command_, sizeof(target_command_));
    r.ReadBytes("pci_sdram_bar", sdram_bar_, sizeof(sdram_bar_));
}

void Vrc5477Pci::ResetInternalFunctions() {
    ac97_.Reset();
    usb_function_.Reset();
    usb_host_.Reset();
}

void Vrc5477Pci::RegisterPciDevice(PciDevice* dev) { external_.RegisterDevice(dev); }

void Vrc5477Pci::RegisterSubtractiveDecoder(PciDevice* dev) { external_.RegisterSubtractiveDecoder(dev); }

void Vrc5477Pci::RegisterBusResetListener(std::function<void()> listener) {
    external_.RegisterBusResetListener(std::move(listener));
}

bool Vrc5477Pci::Owns(uint32_t block_off) {
    const uint32_t dword = block_off & ~3u;
    return FindControlRegister(dword) != nullptr || IsTargetReg(dword);
}

bool Vrc5477Pci::IsBusControlHigh(uint32_t block_off) {
    const uint32_t dword = block_off & ~3u;
    return dword == kOffPciCtl0H || dword == kOffPciCtl1H;
}

uint32_t Vrc5477Pci::ReadReg(uint32_t block_off) {
    if (block_off == kOffSdram0) return sdram_bank_[0];
    if (block_off == kOffSdram1) return sdram_bank_[1];
    for (int i = 0; i < 2; ++i) {
        if (block_off == kOffTargetCmd[i])
            emu_.Get<Fatal>().Die("VRC5477 PCI: target status read off=0x%03X size=4", block_off);
        if (block_off == kOffSdramBar[i]) return sdram_bar_[i];
    }
    const ControlRegister* c = FindControlRegister(block_off);
    if (!c) emu_.Get<Fatal>().Die("VRC5477 PCI: control read off=0x%03X", block_off);
    if (c->upper) return 0u;
    return Channel(c->channel).ReadRegister(c->reg);
}

void Vrc5477Pci::WriteReg(uint32_t block_off, uint32_t value) {
    if (WriteTargetReg(block_off, value)) return;
    const ControlRegister* c = FindControlRegister(block_off);
    if (!c) emu_.Get<Fatal>().Die("VRC5477 PCI: control write off=0x%03X val=0x%08X", block_off, value);
    if (c->upper) Channel(c->channel).WriteUpperWord(c->reg, value);
    else          Channel(c->channel).WriteRegister(c->reg, value);
}

void Vrc5477Pci::BusControlHighWritten(uint32_t block_off, uint32_t value) {
    if ((value & kPciCtlHighWarmReset) != 0u) {
        emu_.Get<Fatal>().Die("VRC5477 PCI: warm reset bit in control-high off=0x%03X val=0x%08X",
                              block_off, value);
    }
    Vrc5477PciChannel& ch = block_off == kOffPciCtl0H ? external_ : internal_;
    ch.SetResetLine((value & kPciCtlHighColdReset) != 0u);
}

Vrc5477PciChannel& Vrc5477Pci::Channel(Vrc5477PciChannelId id) {
    return id == Vrc5477PciChannelId::kExternal ? external_ : internal_;
}

}

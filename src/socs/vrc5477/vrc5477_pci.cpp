#include "vrc5477_pci.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "../../cpu/vr5500/vr5500_id.h"

#include <utility>

namespace cerfcore {

namespace {

using Reg = Vrc5477PciChannel::Register;

constexpr uint32_t kExternalWindowBase   = 0x08000000u;
constexpr uint32_t kExternalWindowSize   = 0x08000000u;
constexpr uint32_t kExternalIoWindowBase = 0x14001000u;
constexpr uint32_t kExternalIoWindowSize = 0x01FFF000u;
constexpr uint32_t kInternalWindowBase   = 0x1D000000u;
constexpr uint32_t kInternalWindowSize   = 0x01000000u;
constexpr uint32_t kInternalIoWindowBase = 0x1C000000u;
constexpr uint32_t kInternalIoWindowSize = 0x01000000u;

constexpr uint32_t kOffPciCtl0H = 0x2E4u;
constexpr uint32_t kOffPciCtl1H = 0x6E4u;

constexpr uint32_t kPciCtlHighColdReset = 0x80000000u;
constexpr uint32_t kPciCtlHighWarmReset = 0x40000000u;

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

}

REGISTER_SERVICE_AS(Vrc5477Pci, PciHostBridge);

Vrc5477Pci::Vrc5477Pci(CerfEmulator& emu)
    : PciHostBridge(emu),
      external_(emu, "external", kExternalWindowBase, kExternalWindowSize, kExternalIoWindowBase,
                kExternalIoWindowSize),
      internal_(emu, "internal", kInternalWindowBase, kInternalWindowSize, kInternalIoWindowBase,
                kInternalIoWindowSize),
      ac97_(emu, kAc97Dev, PciBar{~(kAc97IoSize - 1u), PciBar::kIoSpace, 0u}),
      usb_function_(emu, kUsbFunctionDev, PciBar{}),
      usb_host_(emu, kUsbHostDev, PciBar{~(kOhciMemSize - 1u), 0u, 0u}) {}

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
    });
}

void Vrc5477Pci::ResetInternalFunctions() {
    ac97_.Reset();
    usb_function_.Reset();
    usb_host_.Reset();
}

void Vrc5477Pci::RegisterPciDevice(PciDevice* dev) { external_.RegisterDevice(dev); }

void Vrc5477Pci::RegisterBusResetListener(std::function<void()> listener) {
    external_.RegisterBusResetListener(std::move(listener));
}

bool Vrc5477Pci::Owns(uint32_t block_off) { return FindControlRegister(block_off) != nullptr; }

bool Vrc5477Pci::IsBusControlHigh(uint32_t block_off) {
    return block_off == kOffPciCtl0H || block_off == kOffPciCtl1H;
}

uint32_t Vrc5477Pci::ReadReg(uint32_t block_off) {
    const ControlRegister* c = FindControlRegister(block_off);
    if (!c) emu_.Get<Fatal>().Die("VRC5477 PCI: control read off=0x%03X", block_off);
    if (c->upper) return 0u;
    return Channel(c->channel).ReadRegister(c->reg);
}

void Vrc5477Pci::WriteReg(uint32_t block_off, uint32_t value) {
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

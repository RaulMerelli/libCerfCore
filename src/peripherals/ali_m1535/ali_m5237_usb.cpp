#include "ali_m5237_usb.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../pci/pci_host_bridge.h"

#include <utility>

namespace cerfcore {

namespace {

constexpr OhciProfile kM5237Profile = {
    "ALi M5237",
    0x00000110u,
    0xC000006Fu,
    0x000005FFu,
    0x01000002u,
    0x03001A00u,
    0x00000000u,
};

}

AliM5237Usb::AliM5237Usb(CerfEmulator& emu, const AliM1535InternalFunction& function,
                         std::function<void(bool)> interrupt)
    : emu_(emu), function_(function), interrupt_(std::move(interrupt)), ohci_(emu, kM5237Profile, *this) {}

void AliM5237Usb::Attach() { ohci_.Attach(); }

void AliM5237Usb::Reset() { ohci_.HardwareReset(); }

uint32_t AliM5237Usb::FunctionMemRead(uint32_t offset, unsigned size) {
    if (size != 4u || (offset & 3u) != 0u)
        emu_.Get<Fatal>().Die("ALi M5237 USB: register read off=0x%03X size=%u", offset, size);
    return ohci_.Read(offset);
}

void AliM5237Usb::FunctionMemWrite(uint32_t offset, uint32_t value, unsigned size) {
    if (size != 4u || (offset & 3u) != 0u)
        emu_.Get<Fatal>().Die("ALi M5237 USB: register write off=0x%03X val=0x%08X size=%u", offset, value, size);
    ohci_.Write(offset, value);
}

void AliM5237Usb::RequireBusMaster(uint32_t addr) {
    if (!function_.BusMasterEnabled())
        emu_.Get<Fatal>().Die("ALi M5237 USB: host memory access with bus mastering disabled pci=0x%08X", addr);
}

uint32_t AliM5237Usb::OhciRead32(uint32_t addr) {
    RequireBusMaster(addr);
    return emu_.Get<PciHostBridge>().BusMasterRead32(addr);
}

void AliM5237Usb::OhciWrite32(uint32_t addr, uint32_t value) {
    RequireBusMaster(addr);
    emu_.Get<PciHostBridge>().BusMasterWrite32(addr, value);
}

void AliM5237Usb::OhciInterrupt(bool asserted) { interrupt_(asserted); }

void AliM5237Usb::SaveState(StateWriter& w) { ohci_.SaveState(w); }

void AliM5237Usb::RestoreState(StateReader& r) { ohci_.RestoreState(r); }

void AliM5237Usb::PostRestore() { ohci_.PostRestore(); }

}

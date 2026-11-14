#include "ali_m1535_bridge_config.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

#include <cstring>

namespace cerfcore {

namespace {

constexpr uint32_t kRegChipSelect = 0x54u;

constexpr uint32_t kWrittenDwords[] = {0x40u, 0x44u, 0x48u, 0x50u, 0x54u, 0x58u, 0x6Cu, 0x70u, 0x74u, 0x7Cu};

constexpr uint32_t kRegIoDecode       = 0x44u;
constexpr uint8_t  kIoDecodeSubtract  = 0x40u;
constexpr uint32_t kRegUsbDisable     = 0x53u;
constexpr uint8_t  kUsbDisable        = 0x40u;
constexpr uint32_t kRegIdeControl     = 0x58u;
constexpr uint8_t  kIdeReserved       = 0x80u;
constexpr uint8_t  kIdeEnable         = 0x40u;
constexpr uint8_t  kIdeIdselMask      = 0x30u;
constexpr uint8_t  kIdeIdselAd15      = 0x30u;
constexpr uint32_t kRegIdselMux       = 0x72u;
constexpr uint8_t  kUsbIdselMask      = 0x03u;
constexpr uint8_t  kUsbIdselAd21      = 0x02u;
constexpr uint8_t  kPmuIdselMask      = 0x0Cu;
constexpr uint8_t  kPmuIdselAd28      = 0x00u;
constexpr uint8_t  kPmuIdselCode01    = 0x04u;
constexpr uint8_t  kUsbLevelToEdge    = 0x10u;
constexpr uint32_t kRegModemControl   = 0x77u;
constexpr uint8_t  kModemDisable      = 0x40u;

}

AliM1535BridgeConfig::AliM1535BridgeConfig(CerfEmulator& emu) : emu_(emu) { header_.Reset(); }

void AliM1535BridgeConfig::Reset() {
    header_.Reset();
    std::memset(regs_, 0, sizeof(regs_));
    pm_                  = kPowerManagement;
    chip_select_written_ = false;
}

void AliM1535BridgeConfig::Reject(const char* what, uint32_t reg, uint32_t value) const {
    emu_.Get<Fatal>().Die("ALi M1535+ PCI-ISA AD18: %s reg=0x%02X val=0x%08X", what, reg, value);
}

uint32_t AliM1535BridgeConfig::ConfigRead(uint32_t reg) const {
    uint32_t value = 0u;
    if (header_.Read(reg, &value)) return value;
    if (reg == PciType0Header::kRegCapPtr) return pm_.offset;
    if (reg < kDeviceRegsFirst) return 0u;
    if (pm_.Owns(reg)) return pm_.Read(reg);
    if (reg == kRegChipSelect && !chip_select_written_)
        Reject("chip select and hardware setting read before the guest programs them", reg, 0u);
    return static_cast<uint32_t>(Byte(reg)) | (static_cast<uint32_t>(Byte(reg + 1u)) << 8) |
           (static_cast<uint32_t>(Byte(reg + 2u)) << 16) | (static_cast<uint32_t>(Byte(reg + 3u)) << 24);
}

void AliM1535BridgeConfig::ConfigWrite(uint32_t reg, uint32_t value) {
    if (reg == PciType0Header::kRegSubsystem) Reject("subsystem ID write", reg, value);
    if (pm_.Owns(reg)) {
        if (const char* why = pm_.WriteHoldingD0(reg, value)) Reject(why, reg, value);
        return;
    }
    if (header_.Write(reg, value)) return;
    if (reg >= kDeviceRegsFirst) WriteDeviceDword(reg, value);
}

void AliM1535BridgeConfig::WriteDeviceDword(uint32_t reg, uint32_t value) {
    bool known = false;
    for (uint32_t r : kWrittenDwords) known = known || r == reg;
    if (!known) Reject("device register write that is not modeled", reg, value);
    if (reg == kRegChipSelect) {
        if (value != 0u) Reject("chip select or hardware setting value that is not modeled", reg, value);
        chip_select_written_ = true;
    }

    const auto byte_of = [&](uint32_t r) { return static_cast<uint8_t>(value >> ((r - reg) * 8u)); };
    if (reg == (kRegIoDecode & ~3u) && (byte_of(kRegIoDecode) & kIoDecodeSubtract) == 0u)
        Reject("positive on-chip I/O decode", reg, value);
    if (reg == (kRegIdeControl & ~3u)) {
        const uint8_t ide = byte_of(kRegIdeControl);
        if ((ide & kIdeReserved) != 0u || ((ide & kIdeEnable) != 0u && (ide & kIdeIdselMask) != kIdeIdselAd15))
            Reject("IDE control setting that is not modeled", reg, value);
    }
    if (reg == (kRegUsbRouting & ~3u) && (byte_of(kRegUsbRouting) & kUsbLevelToEdge) != 0u)
        Reject("USB interrupt level-to-edge transform", reg, value);
    if (reg == (kRegIdselMux & ~3u)) {
        const uint8_t mux = byte_of(kRegIdselMux);
        const uint8_t pmu = mux & kPmuIdselMask;
        if ((mux & kUsbIdselMask) != kUsbIdselAd21 || (pmu != kPmuIdselAd28 && pmu != kPmuIdselCode01))
            Reject("USB or PMU IDSEL selection that is not modeled", reg, value);
    }
    for (uint32_t i = 0; i < 4u; ++i) regs_[reg - kDeviceRegsFirst + i] = byte_of(reg + i);
}

bool AliM1535BridgeConfig::Visible(AliM1535Function f) const {
    if (f == AliM1535Function::kIde) return (Byte(kRegIdeControl) & kIdeEnable) != 0u;
    if (f == AliM1535Function::kAudio) return true;
    if (f == AliM1535Function::kModem) return (Byte(kRegModemControl) & kModemDisable) == 0u;
    return (Byte(kRegIdselMux) & kUsbIdselMask) == kUsbIdselAd21 && (Byte(kRegUsbDisable) & kUsbDisable) == 0u;
}

void AliM1535BridgeConfig::SaveState(StateWriter& w) {
    w.WriteBytes("bridge_regs", regs_, sizeof(regs_));
    w.Write("bridge_chip_select_written", chip_select_written_);
    StateWriteField field(w);
    PciType0Header::Visit(header_, field);
    PciPowerManagement::Visit(pm_, field);
}

void AliM1535BridgeConfig::RestoreState(StateReader& r) {
    r.ReadBytes("bridge_regs", regs_, sizeof(regs_));
    r.Read("bridge_chip_select_written", chip_select_written_);
    StateReadField field(r);
    PciType0Header::Visit(header_, field);
    PciPowerManagement::Visit(pm_, field);
}

}

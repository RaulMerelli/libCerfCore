#pragma once

#include "../pci/pci_power_management.h"
#include "../pci/pci_type0_header.h"

#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

enum class AliM1535Function { kIde, kAudio, kModem, kUsb };

class AliM1535BridgeConfig {
public:
    explicit AliM1535BridgeConfig(CerfEmulator& emu);

    void Reset();

    uint32_t ConfigRead(uint32_t reg) const;
    void     ConfigWrite(uint32_t reg, uint32_t value);

    bool    Visible(AliM1535Function f) const;
    uint8_t UsbInterruptRouting() const { return Byte(kRegUsbRouting); }

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

private:
    static constexpr uint32_t kDeviceRegsFirst = 0x40u;
    static constexpr uint32_t kDeviceRegsCount = 0xC0u;
    static constexpr uint32_t kRegUsbRouting   = 0x74u;
    static constexpr PciPowerManagement kPowerManagement{0xA0u, 0x00010001u};
    static constexpr PciHeaderIdentity  kHeaderIdentity{
        {0x153310B9u, 0x06010000u, 0x0210u, 0x000Fu, 0x0000u, 0x00u, 0x00u, 0x00u}, 0x00u, 0x00u, 0x00000000u};

    uint8_t Byte(uint32_t reg) const { return regs_[reg - kDeviceRegsFirst]; }
    void    WriteDeviceDword(uint32_t reg, uint32_t value);

    [[noreturn]] void Reject(const char* what, uint32_t reg, uint32_t value) const;

    CerfEmulator&      emu_;
    PciType0Header     header_{kHeaderIdentity};
    uint8_t            regs_[kDeviceRegsCount] = {};
    PciPowerManagement pm_                     = kPowerManagement;
    bool               chip_select_written_    = false;
};

}

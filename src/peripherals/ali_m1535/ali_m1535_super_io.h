#pragma once

#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

class AliM1535SuperIo {
public:
    static constexpr uint32_t kConfigPort      = 0x3F0u;
    static constexpr uint32_t kDataPort        = 0x3F1u;
    static constexpr uint32_t kInternalIoLimit = 0x1000u;

    explicit AliM1535SuperIo(CerfEmulator& emu);

    void Reset();

    static bool Decodes(uint32_t port) { return port == kConfigPort || port == kDataPort; }
    uint8_t     Read(uint32_t port);
    void        Write(uint32_t port, uint8_t value);

    uint8_t KeyboardIrq() const { return keyboard_.irq; }
    uint8_t MouseIrq() const { return keyboard_.irq2; }

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

private:
    enum Mode : uint8_t { kNormal, kKeyHalf, kConfig };

    struct LogicalDevice {
        uint8_t activate = 0;
        uint8_t base_hi  = 0;
        uint8_t base_lo  = 0;
        uint8_t irq      = 0;
        uint8_t irq2     = 0;
    };

    LogicalDevice* Selected();
    void           WriteData(uint8_t value);
    [[noreturn]] void Reject(const char* what, uint32_t port, uint8_t value);

    CerfEmulator& emu_;
    uint8_t       mode_  = kNormal;
    uint8_t       index_ = 0;
    uint8_t       ldn_   = 0;
    LogicalDevice parallel_;
    LogicalDevice uart1_;
    LogicalDevice uart2_;
    LogicalDevice keyboard_;
    LogicalDevice uart3_;
};

}

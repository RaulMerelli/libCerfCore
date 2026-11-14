#include "ali_m1535_super_io.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

#include <initializer_list>

namespace cerfcore {

namespace {

constexpr uint8_t kEnterKey1 = 0x51u;
constexpr uint8_t kEnterKey2 = 0x23u;
constexpr uint8_t kExitKey   = 0xBBu;

constexpr uint8_t kIndexLogicalDevice = 0x07u;
constexpr uint8_t kIndexActivate      = 0x30u;
constexpr uint8_t kIndexBaseHigh      = 0x60u;
constexpr uint8_t kIndexBaseLow       = 0x61u;
constexpr uint8_t kIndexIrq           = 0x70u;
constexpr uint8_t kIndexIrq2          = 0x72u;

constexpr uint8_t kLdnParallel = 0x03u;
constexpr uint8_t kLdnUart1    = 0x04u;
constexpr uint8_t kLdnUart2    = 0x05u;
constexpr uint8_t kLdnKeyboard = 0x07u;
constexpr uint8_t kLdnUart3    = 0x0Bu;

constexpr uint8_t kActivateBit      = 0x01u;
constexpr uint8_t kIrqSelectMask    = 0x0Fu;
constexpr uint8_t kParallelBaseLow0 = 0x03u;
constexpr uint8_t kUartBaseLow0     = 0x07u;

}

AliM1535SuperIo::AliM1535SuperIo(CerfEmulator& emu) : emu_(emu) { Reset(); }

void AliM1535SuperIo::Reset() {
    mode_     = kNormal;
    index_    = 0u;
    ldn_      = 0u;
    parallel_ = LogicalDevice{0x00u, 0x03u, 0x78u, 0x05u, 0x00u};
    uart1_    = LogicalDevice{0x00u, 0x03u, 0xF8u, 0x04u, 0x00u};
    uart2_    = LogicalDevice{0x00u, 0x03u, 0xE8u, 0x09u, 0x00u};
    keyboard_ = LogicalDevice{kActivateBit, 0x00u, 0x00u, 0x01u, 0x00u};
    uart3_    = LogicalDevice{};
}

void AliM1535SuperIo::Reject(const char* what, uint32_t port, uint8_t value) {
    emu_.Get<Fatal>().Die("ALi M1535+ Super I/O: %s port=0x%03X val=0x%02X mode=%u index=0x%02X ldn=0x%02X", what,
                          port, value, mode_, index_, ldn_);
}

uint8_t AliM1535SuperIo::Read(uint32_t port) { Reject("read", port, 0u); }

void AliM1535SuperIo::Write(uint32_t port, uint8_t value) {
    switch (mode_) {
        case kNormal:
            if (port != kConfigPort || value != kEnterKey1) Reject("write outside configuration mode", port, value);
            mode_ = kKeyHalf;
            return;
        case kKeyHalf:
            if (port != kConfigPort || value != kEnterKey2) Reject("broken configuration key", port, value);
            mode_ = kConfig;
            return;
        default:
            break;
    }
    if (port == kDataPort) {
        WriteData(value);
        return;
    }
    if (value == kExitKey) {
        mode_ = kNormal;
        return;
    }
    index_ = value;
}

AliM1535SuperIo::LogicalDevice* AliM1535SuperIo::Selected() {
    switch (ldn_) {
        case kLdnParallel: return &parallel_;
        case kLdnUart1:    return &uart1_;
        case kLdnUart2:    return &uart2_;
        case kLdnKeyboard: return &keyboard_;
        case kLdnUart3:    return &uart3_;
        default:           return nullptr;
    }
}

void AliM1535SuperIo::WriteData(uint8_t value) {
    if (index_ == kIndexLogicalDevice) {
        ldn_ = value;
        return;
    }
    LogicalDevice* dev = Selected();
    if (!dev) Reject("data write to a logical device that is not modeled", kDataPort, value);
    const bool keyboard = dev == &keyboard_;
    switch (index_) {
        case kIndexActivate:
            if ((value & ~kActivateBit) != 0u) Reject("activate register bits that are not modeled", kDataPort, value);
            if (keyboard && value != kActivateBit) Reject("keyboard controller deactivation", kDataPort, value);
            dev->activate = value;
            return;
        case kIndexBaseHigh:
            if (keyboard) break;
            if ((static_cast<uint32_t>(value) << 8) >= kInternalIoLimit)
                Reject("base address outside the internal decode range", kDataPort, value);
            dev->base_hi = value;
            return;
        case kIndexBaseLow:
            if (keyboard) break;
            if ((value & (dev == &parallel_ ? kParallelBaseLow0 : kUartBaseLow0)) != 0u)
                Reject("base address low bits that must be zero", kDataPort, value);
            dev->base_lo = value;
            return;
        case kIndexIrq:
            if ((value & ~kIrqSelectMask) != 0u) Reject("IRQ select upper bits", kDataPort, value);
            dev->irq = value;
            return;
        case kIndexIrq2:
            if (!keyboard) break;
            if ((value & ~kIrqSelectMask) != 0u) Reject("mouse IRQ select upper bits", kDataPort, value);
            dev->irq2 = value;
            return;
        default:
            break;
    }
    Reject("data write to a register that is not modeled", kDataPort, value);
}

void AliM1535SuperIo::SaveState(StateWriter& w) {
    w.Write("sio_mode", mode_);
    w.Write("sio_index", index_);
    w.Write("sio_ldn", ldn_);
    for (LogicalDevice* d : {&parallel_, &uart1_, &uart2_, &keyboard_, &uart3_}) {
        w.Write("sio_activate", d->activate);
        w.Write("sio_base_hi", d->base_hi);
        w.Write("sio_base_lo", d->base_lo);
        w.Write("sio_irq", d->irq);
        w.Write("sio_irq2", d->irq2);
    }
}

void AliM1535SuperIo::RestoreState(StateReader& r) {
    r.Read("sio_mode", mode_);
    r.Read("sio_index", index_);
    r.Read("sio_ldn", ldn_);
    for (LogicalDevice* d : {&parallel_, &uart1_, &uart2_, &keyboard_, &uart3_}) {
        r.Read("sio_activate", d->activate);
        r.Read("sio_base_hi", d->base_hi);
        r.Read("sio_base_lo", d->base_lo);
        r.Read("sio_irq", d->irq);
        r.Read("sio_irq2", d->irq2);
    }
}

}

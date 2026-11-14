#include <array>
#include <cstdint>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "sh7729_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kWindowPa   = 0x05000000u;
constexpr uint32_t kWindowSize = 0x01000000u;
constexpr uint32_t kImageMask  = 0x0001FFFFu;
constexpr uint32_t kHalfShift  = 16u;
constexpr uint32_t kHalfMask   = 0x0000FFFFu;
constexpr uint32_t kRamOffset  = 0x7000u;
constexpr uint32_t kRamBytes   = 0x2000u;
constexpr uint32_t kHalves     = 2u;
constexpr uint32_t kXBusZeroPa = kWindowPa;
constexpr uint32_t kYBusZeroPa = kWindowPa + (1u << kHalfShift);

class Sh7729XyMemory : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7729; }

    void OnReady() override { emu_.Get<PeripheralDispatcher>().Register(this); }

    uint32_t MmioBase() const override { return kWindowPa; }
    uint32_t MmioSize() const override { return kWindowSize; }

    uint8_t  ReadByte(uint32_t addr) override { return *Ram("ReadByte", addr, 0u); }
    uint16_t ReadHalf(uint32_t addr) override { return cerf::le::U16(Ram("ReadHalf", addr, 0u)); }
    uint32_t ReadWord(uint32_t addr) override { return cerf::le::U32(Ram("ReadWord", addr, 0u)); }
    void     WriteByte(uint32_t addr, uint8_t value) override { *Ram("WriteByte", addr, value) = value; }
    void     WriteWord(uint32_t addr, uint32_t value) override {
        cerf::le::Put32(Ram("WriteWord", addr, value), value);
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        if (addr == kXBusZeroPa || addr == kYBusZeroPa) return;
        cerf::le::Put16(Ram("WriteHalf", addr, value), value);
    }

    void SaveState(StateWriter& w) override {
        w.WriteBytes("x_ram", ram_[0].data(), kRamBytes);
        w.WriteBytes("y_ram", ram_[1].data(), kRamBytes);
    }

    void RestoreState(StateReader& r) override {
        r.ReadBytes("x_ram", ram_[0].data(), kRamBytes);
        r.ReadBytes("y_ram", ram_[1].data(), kRamBytes);
    }

private:
    uint8_t* RamOrNull(uint32_t addr) {
        const uint32_t image = addr & kImageMask;
        const uint32_t index = (image & kHalfMask) - kRamOffset;
        return index < kRamBytes ? &ram_[image >> kHalfShift][index] : nullptr;
    }

    uint8_t* Ram(const char* access, uint32_t addr, uint32_t value) {
        uint8_t* ram = RamOrNull(addr);
        if (ram == nullptr) HaltUnsupportedAccess(access, addr, value);
        return ram;
    }

    std::array<std::array<uint8_t, kRamBytes>, kHalves> ram_{};
};

}

REGISTER_SERVICE(Sh7729XyMemory);

}

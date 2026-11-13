#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../cpu/vr5500/vr5500_id.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "vrc5477_pci.h"

#include <cstdint>

namespace cerfcore {

namespace {

template <Vrc5477PciChannelId kChannel>
class Vrc5477PciIoWindow : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == SocId::Vr5500;
    }
    void OnReady() override {
        channel_ = &emu_.Get<Vrc5477Pci>().Channel(kChannel);
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return channel_->IoWindowBase(); }
    uint32_t MmioSize() const override { return channel_->IoWindowSize(); }

    uint8_t  ReadByte(uint32_t a) override { return static_cast<uint8_t>(channel_->IoRead(a, 1)); }
    uint16_t ReadHalf(uint32_t a) override { return static_cast<uint16_t>(channel_->IoRead(a, 2)); }
    uint32_t ReadWord(uint32_t a) override { return channel_->IoRead(a, 4); }
    void WriteByte(uint32_t a, uint8_t v) override { channel_->IoWrite(a, v, 1); }
    void WriteHalf(uint32_t a, uint16_t v) override { channel_->IoWrite(a, v, 2); }
    void WriteWord(uint32_t a, uint32_t v) override { channel_->IoWrite(a, v, 4); }

private:
    Vrc5477PciChannel* channel_ = nullptr;
};

using Vrc5477ExternalPciIoWindow = Vrc5477PciIoWindow<Vrc5477PciChannelId::kExternal>;
using Vrc5477InternalPciIoWindow = Vrc5477PciIoWindow<Vrc5477PciChannelId::kInternal>;

REGISTER_SERVICE(Vrc5477ExternalPciIoWindow);
REGISTER_SERVICE(Vrc5477InternalPciIoWindow);

}

}

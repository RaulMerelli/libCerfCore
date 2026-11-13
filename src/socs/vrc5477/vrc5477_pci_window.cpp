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
class Vrc5477PciWindow : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == SocId::Vr5500;
    }
    void OnReady() override {
        channel_ = &emu_.Get<Vrc5477Pci>().Channel(kChannel);
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return channel_->WindowBase(); }
    uint32_t MmioSize() const override { return channel_->WindowSize(); }

    uint8_t  ReadByte(uint32_t a) override { return static_cast<uint8_t>(channel_->Read(a, 1)); }
    uint16_t ReadHalf(uint32_t a) override { return static_cast<uint16_t>(channel_->Read(a, 2)); }
    uint32_t ReadWord(uint32_t a) override { return channel_->Read(a, 4); }
    void WriteByte(uint32_t a, uint8_t v) override { channel_->Write(a, v, 1); }
    void WriteHalf(uint32_t a, uint16_t v) override { channel_->Write(a, v, 2); }
    void WriteWord(uint32_t a, uint32_t v) override { channel_->Write(a, v, 4); }

    uint64_t ReadDword(uint32_t a) override { return channel_->ReadDword(a); }
    void WriteDword(uint32_t a, uint64_t v) override { channel_->WriteDword(a, v); }

    void SaveState(StateWriter& w) override { channel_->SaveState(w); }
    void RestoreState(StateReader& r) override { channel_->RestoreState(r); }
    void PostRestore() override { channel_->PostRestore(); }

private:
    Vrc5477PciChannel* channel_ = nullptr;
};

using Vrc5477ExternalPciWindow = Vrc5477PciWindow<Vrc5477PciChannelId::kExternal>;
using Vrc5477InternalPciWindow = Vrc5477PciWindow<Vrc5477PciChannelId::kInternal>;

REGISTER_SERVICE(Vrc5477ExternalPciWindow);
REGISTER_SERVICE(Vrc5477InternalPciWindow);

}

}

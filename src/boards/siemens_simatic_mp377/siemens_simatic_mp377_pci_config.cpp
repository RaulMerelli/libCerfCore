#include "../../socs/iop13xx/iop13xx_pci_config.h"

#include "../../peripherals/pci/pci_bar.h"
#include "../../peripherals/pci/pci_type0_header.h"
#include "../../peripherals/siemens_ertec400/siemens_simatic_mp377_ertec400.h"
#include "../../peripherals/silicon_motion_sm501/siemens_simatic_mp377_sm501.h"
#include <cerfcore/boards/board_context.h>
#include "siemens_simatic_mp377_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>
#include <cerfcore/socs/guest_cpu_reset.h>

#include <cstddef>
#include <cstdint>

namespace cerfcore {

namespace {

namespace sm = siemens_simatic_mp377;

constexpr uint32_t kConfigOffsetMask = 0xFCu;
constexpr uint8_t  kByteWritable     = 0xFFu;
constexpr uint16_t kCommandWritable  = 0xFFFFu;
constexpr uint32_t kPinGntLatMask    = 0xFFFFFF00u;
constexpr size_t   kSm501FbBar       = 0u;
constexpr size_t   kSm501RegsBar     = 1u;

constexpr PciHeaderIdentity kSm501Identity{
    {sm::kSm501PciDeviceVendorDword, sm::kSm501PciClassDisplayDword,
     static_cast<uint16_t>(sm::kSm501PciCommandStatusDword >> 16), static_cast<uint16_t>(sm::kSm501PciCommandStatusDword),
     kCommandWritable, kByteWritable, kByteWritable, static_cast<uint8_t>(sm::kSm501PciHeaderTypeDword >> 16)},
    static_cast<uint8_t>(sm::kSm501PciInterruptPinIntaLine0Dword), kByteWritable,
    sm::kSm501PciInterruptPinIntaLine0Dword & kPinGntLatMask};

constexpr uint32_t          kErtec400IdentityDword = 0x4026110Au;
constexpr PciHeaderIdentity kErtec400Identity{
    {kErtec400IdentityDword, 0x02000000u, 0x0200u, 0x0007u, kCommandWritable, kByteWritable, kByteWritable, 0x00u},
    0x00u, kByteWritable, 0x00000100u};

class SiemensSimaticMp377PciConfig final : public Iop13xxPciConfig {
public:
    using Iop13xxPciConfig::Iop13xxPciConfig;

    bool ShouldRegister() override {
        auto* board = emu_.TryGet<BoardContext>();
        return board && board->GetBoardFamilyId() == BoardFamilyId::SiemensSimaticMp377;
    }

    void OnReady() override {
        sm501_.SetBarReset(kSm501FbBar, PciBar{sm::kSm501PciFbBarSizeMask, sm::kSm501PciFbBarFlags,
                                               sm::kSm501PciFbBarDword & sm::kSm501PciFbBarSizeMask});
        sm501_.SetBarReset(kSm501RegsBar, PciBar{sm::kSm501PciRegsBarSizeMask, sm::kSm501PciRegsBarFlags,
                                                 sm::kSm501PciRegsBarDword & sm::kSm501PciRegsBarSizeMask});
        for (size_t bar = 0; bar < PciType0Header::kBars; ++bar)
            ertec_.SetBarReset(bar, PciBar{Ertec400BarMask(bar), 0u, Ertec400ModelBase(bar)});
        Reset();
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            if (!emu_.Get<GuestCpuReset>().DeliveredResetWasResume()) Reset();
        });
    }

    void Reset() {
        sm501_.Reset();
        ertec_.Reset();
    }

    uint32_t ReadPrimary(uint32_t occar) override {
        const uint32_t select = DeviceSelect(occar);
        if (select == kSm501Select) return Sm501ConfigRead(occar & kConfigOffsetMask);
        if (select == kErtec400Select) return Ertec400ConfigRead(occar & kConfigOffsetMask);
        return 0xFFFFFFFFu;
    }

    bool WritePrimary(uint32_t occar, uint32_t value) override {
        const uint32_t select = DeviceSelect(occar);
        if (select == kErtec400Select) return WriteHeader(ertec_, occar & kConfigOffsetMask, value);
        if (select != kSm501Select) return true;
        return WriteHeader(sm501_, occar & kConfigOffsetMask, value);
    }

    uint32_t ReadSecondary(uint32_t occar) override {
        if (DeviceSelect(occar) != kSecondarySm501Select) return 0xFFFFFFFFu;
        return Sm501ConfigRead(occar & kConfigOffsetMask);
    }
    bool WriteSecondary(uint32_t occar, uint32_t value) override {
        if (DeviceSelect(occar) != kSecondarySm501Select) return true;
        return WriteHeader(sm501_, occar & kConfigOffsetMask, value);
    }

    Iop13xxPciMemoryMapResult MapMemoryBar(uint32_t pci_address, uint32_t size,
                                           uint32_t& model_address) const override {
        if ((sm501_.Command() & PciCommonHeader::kCommandMemory) != 0u) {
            if (MapPciBar(sm501_.Bar(kSm501FbBar), pci_address, size, sm::kSm501FbBarPa, model_address))
                return Iop13xxPciMemoryMapResult::kMapped;
            if (MapPciBar(sm501_.Bar(kSm501RegsBar), pci_address, size, sm::kSm501RegsBarPa, model_address))
                return Iop13xxPciMemoryMapResult::kMapped;
        }
        if ((ertec_.Command() & PciCommonHeader::kCommandMemory) != 0u) {
            for (size_t bar = 0; bar < PciType0Header::kBars; ++bar) {
                if (MapPciBar(ertec_.Bar(bar), pci_address, size, Ertec400ModelBase(bar), model_address))
                    return Iop13xxPciMemoryMapResult::kMapped;
            }
        }
        if (InModelAperture(pci_address, size)) return Iop13xxPciMemoryMapResult::kUnmapped;
        return Iop13xxPciMemoryMapResult::kNotBar;
    }

    void SaveState(StateWriter& writer) override {
        StateWriteField field(writer);
        PciType0Header::Visit(sm501_, field);
        PciType0Header::Visit(ertec_, field);
    }

    void RestoreState(StateReader& reader) override {
        StateReadField field(reader);
        PciType0Header::Visit(sm501_, field);
        PciType0Header::Visit(ertec_, field);
    }

private:
    static uint32_t DeviceSelect(uint32_t occar) { return occar & 0x7FFFFF00u; }

    uint32_t Sm501ConfigRead(uint32_t off) const {
        uint32_t value = 0u;
        if (sm501_.Read(off, &value)) return value;
        if (off == PciType0Header::kRegSubsystem) return sm::kSm501PciSubsystemDword;
        if (off == PciType0Header::kRegCapPtr) return sm::kSm501PciCapabilityPointerDword;
        return 0u;
    }

    uint32_t Ertec400ConfigRead(uint32_t off) const {
        uint32_t value = 0u;
        if (ertec_.Read(off, &value)) return value;
        if (off == PciType0Header::kRegSubsystem) return kErtec400IdentityDword;
        return 0u;
    }

    static bool HeaderWriteHasNoEffect(uint32_t off) {
        return off == PciCommonHeader::kRegIdentity || off == PciCommonHeader::kRegClass ||
               (off >= PciType0Header::kRegCardbusCis && off < PciType0Header::kRegInterrupt);
    }

    static bool WriteHeader(PciType0Header& header, uint32_t off, uint32_t value) {
        return header.Write(off, value) || HeaderWriteHasNoEffect(off);
    }

    static constexpr uint32_t Ertec400BarMask(size_t bar) {
        return bar == 3u ? 0xFF800000u : 0xFFFF0000u;
    }

    static constexpr uint32_t Ertec400BarSize(size_t bar) {
        return bar == 3u ? 0x00800000u : 0x00010000u;
    }

    static constexpr uint32_t Ertec400ModelBase(size_t bar) {
        switch (bar) {
        case 0u: return sm::kErtecBar0Base;
        case 1u: return sm::kErtecBar1Base;
        case 2u: return sm::kErtecBar2Base;
        case 3u: return sm::kErtecBar3Base;
        case 4u: return sm::kErtecBar4Base;
        case 5u: return sm::kErtecBar5Base;
        default: return 0u;
        }
    }

    static bool MapBar(uint32_t address, uint32_t access_size, uint32_t configured_bar,
                       uint32_t bar_size, uint32_t model_base, uint32_t& model_address) {
        if (access_size == 0u) return false;
        const uint32_t base = configured_bar & ~(bar_size - 1u);
        if (address < base) return false;
        const uint64_t offset = static_cast<uint64_t>(address) - base;
        if (offset + access_size > bar_size) return false;
        model_address = model_base + static_cast<uint32_t>(offset);
        return true;
    }

    static bool MapPciBar(const PciBar& bar, uint32_t address, uint32_t access_size, uint32_t model_base,
                          uint32_t& model_address) {
        if (access_size == 0u || !bar.Decodes(address) || !bar.Decodes(address + access_size - 1u)) return false;
        model_address = model_base + (address - bar.base);
        return true;
    }

    static bool InModelAperture(uint32_t address, uint32_t access_size) {
        uint32_t ignored = 0u;
        if (MapBar(address, access_size, sm::kSm501FbBarPa, sm::kSm501FbBytes, 0u, ignored)) return true;
        if (MapBar(address, access_size, sm::kSm501RegsBarPa, sm::kSm501RegsBytes, 0u, ignored)) return true;
        for (size_t bar = 0; bar < PciType0Header::kBars; ++bar) {
            if (MapBar(address, access_size, Ertec400ModelBase(bar), Ertec400BarSize(bar), 0u, ignored)) return true;
        }
        return false;
    }

    static constexpr uint32_t kSm501Select = 0x00007800u;
    static constexpr uint32_t kSecondarySm501Select = 0x00007800u;
    static constexpr uint32_t kErtec400Select = sm::kErtec400PciSelect;

    PciType0Header sm501_{kSm501Identity};
    PciType0Header ertec_{kErtec400Identity};
};

REGISTER_SERVICE_AS(SiemensSimaticMp377PciConfig, Iop13xxPciConfig);

}

}

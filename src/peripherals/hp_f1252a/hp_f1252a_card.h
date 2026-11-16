#pragma once

#include "../pcmcia/pcmcia_card.h"
#include "vga_controller.h"

#include <cstdint>

namespace cerfcore {

class HpF1252aCard : public PcmciaCard {
public:
    static constexpr const char* kDisplayName = "HP VGA out PC Card (F1252A)";

    explicit HpF1252aCard(CerfEmulator& emu);
    ~HpF1252aCard() override;

    std::string DisplayName() const override { return kDisplayName; }
    PcmciaVoltageSense VoltageSense() const override { return { false, false, false }; }
    bool HoldsReadyBusyInReset() const override { return false; }

    void OnInserted() override;
    void OnShutdown() override;

    void PowerOn () override;

    const char* SaveId() const override { return "hpvga"; }
    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PowerOff() override;

    uint8_t  ReadAttribute8 (uint32_t offset)                override;
    void     WriteAttribute8(uint32_t offset, uint8_t value) override;

    uint8_t  ReadCommon8  (uint32_t offset)                  override;
    uint16_t ReadCommon16 (uint32_t offset)                  override;
    void     WriteCommon8 (uint32_t offset, uint8_t  value)  override;
    void     WriteCommon16(uint32_t offset, uint16_t value)  override;

    uint8_t  ReadIo8  (uint32_t offset)                      override;
    uint16_t ReadIo16 (uint32_t offset)                      override;
    void     WriteIo8 (uint32_t offset, uint8_t  value)      override;
    void     WriteIo16(uint32_t offset, uint16_t value)      override;
    bool     Iois16   (uint32_t)                             override { return true; }
    bool     InputAcknowledge(uint32_t)                      override { return false; }

private:
    void SyncDisplayToMode();
    void DetachDisplay();

    static constexpr uint32_t kRegEnd    = 0x0046E9u;
    static constexpr uint32_t kMagicBase = 0x0C0000u;
    static constexpr uint32_t kMagicLen  = 8u;

    bool InMagic(uint32_t offset, uint32_t bytes) const;
    void RequireOutsideMagic(const char* access, uint32_t offset, uint32_t bytes);
    bool LinearByte(uint32_t offset, uint32_t& at) const;
    bool LinearWord(uint32_t offset, uint32_t& at) const;

    [[noreturn]] void HaltMagic(const char* access, uint32_t offset);
    [[noreturn]] void HaltUnmapped(const char* space, const char* access, uint32_t offset);

    VgaController controller_;
    int           display_id_ = 0;

    uint32_t last_w_ = 0, last_h_ = 0;
};

}

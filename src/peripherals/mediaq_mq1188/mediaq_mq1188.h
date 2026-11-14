#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../lcd/display_mode_latch.h"
#include "../usb/ohci_host_controller.h"
#include "mediaq_mq1188_ge.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

/* MediaQ MQ-1100/1132 ("MQ1188" on Falcon PC3xx, PXA255 static CS2, aperture
   PA 0x08000000). Ref: MediaQ doc 12-00026 Rev D, Ch.4. The 512 KB aperture
   is frame-buffer SRAM except an 8 KB register window at +0x40000 (Table 4-1
   / Fig 4-1) - only that window has device semantics. */
class MediaQMq1188 : public Peripheral, public MediaQGeHost, public OhciHostBus {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override { return 0x08000000u; }
    uint32_t MmioSize() const override { return kApertureSize; }

    uint8_t  ReadByte (uint32_t addr) override;
    uint16_t ReadHalf (uint32_t addr) override;
    uint32_t ReadWord (uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t  value) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;
    void     WriteWord(uint32_t addr, uint32_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

    uint32_t OhciRead32(uint32_t addr) override;
    void     OhciWrite32(uint32_t addr, uint32_t value) override;
    void     OhciInterrupt(bool asserted) override;

    /* Getters consumed by MediaQMq1188Renderer. All values come from the
       Graphics Controller register block (Table 4-9) and Device Config
       (Table 4-2); register identities confirmed against ddi.dll's mode-set
       (sub_18414F8 / sub_18415A0 / sub_1841990). */
    bool     IsEnabled()      const;   /* DC05R[0] display-on AND a valid mode. */
    uint32_t Bpp()            const;   /* decoded from GC00R[7:4]. */
    uint32_t GetGuestW()      const;   /* GC08R[31:16] + 1. */
    uint32_t GetGuestH()      const;   /* GC09R[31:16] + 1. */
    uint32_t FbWindowOffset() const;   /* GC0CR[17:0], byte offset in FB SRAM. */
    uint32_t Stride()         const;   /* GC0ER[17:0], bytes per scanline. */
    uint32_t PaletteEntry(uint32_t index) const;   /* Color Palette 0x800-0xBFF. */
    const uint8_t* FbBytes()  const { return sram_.data(); }
    uint8_t*       FbMutableBytes() override { return sram_.data(); }   /* GE blit target. */
    uint32_t       FbSize()   const override { return kSramSize; }
    Fatal&         OwnerFatal() override;

private:
    static constexpr uint32_t kApertureSize = 0x00080000u;  /* 512 KB total. */
    static constexpr uint32_t kSramSize     = 0x00040000u;
    static constexpr uint32_t kRegBase      = 0x00040000u;  /* registers at +0x40000. */
    static constexpr uint32_t kRegWindow    = 0x00002000u;  /* 8 KB register space. */

    /* Graphics Controller register chip-offsets (Table 4-9). */
    static constexpr uint32_t kGc00R = 0x180u;  /* control: [0] enable, [7:4] color depth. */
    static constexpr uint32_t kGc08R = 0x1A0u;  /* [31:16] = horizontal active - 1. */
    static constexpr uint32_t kGc09R = 0x1A4u;  /* [31:16] = vertical active - 1. */
    static constexpr uint32_t kGc0CR = 0x1B0u;  /* window start address [17:0]. */
    static constexpr uint32_t kGc0ER = 0x1B8u;  /* window stride [17:0]. */

    static constexpr uint32_t kDc01R = 0x384u;  /* reset value 0xF0000000 (Table 4-2). */
    static constexpr uint32_t kDc05R = 0x394u;  /* [0] = display on/off (driver sub_1841990). */

    static constexpr uint32_t kPaletteBase = 0x800u;  /* 256 entries x 4 bytes. */

    /* Graphics Engine apertures (Table 4-1 + queued command alias). */
    static constexpr uint32_t kCc01R       = 0x004u;   /* FIFO/GE status (Reg 4-10). */
    /* The GE is accessed only via the queued command alias at 0x1400. The 0x200
       direct block (MQ-1132 GE registers) is the MQ-1188 SD/MMC controller. */
    static constexpr uint32_t kGeCmdLo     = 0x1400u;  /* queued command alias. */
    static constexpr uint32_t kGeCmdHi     = 0x1480u;
    static constexpr uint32_t kSrcFifoLo   = 0xC00u;   /* Source FIFO Space. */
    static constexpr uint32_t kSrcFifoHi   = 0x1000u;

    /* USB Host OHCI operational registers (Table 4-1: 0x500-0x5FF). */
    static constexpr uint32_t kUsbLo = 0x500u, kUsbHi = 0x5FFu;

    static constexpr uint32_t kCc02R             = 0x008u;
    static constexpr uint32_t kIn00R             = 0x100u;
    static constexpr uint32_t kIn01R             = 0x104u;
    static constexpr uint32_t kIn02R             = 0x108u;
    static constexpr uint32_t kIn03R             = 0x10Cu;
    static constexpr uint32_t kIn00rEnable       = 0x1u;
    static constexpr uint32_t kIn00rActiveHigh   = 0x2u;
    static constexpr uint32_t kIn00rGpioLevel0   = 0x4u;
    static constexpr uint32_t kIntGpio0          = 0x20u;
    static constexpr uint32_t kIntUsbHost        = 0x8000u;
    static constexpr uint32_t kIntLatched        = 0x00003FFFu;
    static constexpr uint32_t kIntDefined        = 0x000FFFFFu;
    static constexpr uint32_t kCc02rGpioField    = 4u;
    static constexpr uint32_t kCc02rInputEnable  = 0x1u;
    static constexpr uint32_t kCc02rOutputEnable = 0x2u;
    static constexpr uint32_t kCc02rTestSource   = 0x4u;
    static constexpr uint32_t kCc02rData         = 0x8u;
    static constexpr uint32_t kMqGpioPins        = 3u;
    static constexpr uint32_t kDc05rUsbHostClock = 0x2u;

    static constexpr uint32_t kCc01rFifoFree     = 0x1Fu;
    static constexpr uint32_t kCc01rSourceShift  = 8u;
    static constexpr uint32_t kCc01rGeBusy       = 0x10000u;
    static constexpr uint32_t kFifoDepth         = 16u;
    static constexpr uint32_t kRawGpio0          = 0x10u;
    static constexpr uint32_t kRawGeBusy         = 0x100u;
    static constexpr uint32_t kRawSourceEmpty    = 0x200u;
    static constexpr uint32_t kRawSourceHalf     = 0x400u;
    static constexpr uint32_t kRawCommandEmpty   = 0x800u;
    static constexpr uint32_t kRawCommandHalf    = 0x1000u;
    static constexpr uint32_t kRawUsbHost        = 0x2000u;

    static const OhciProfile kUsbProfile;

    static bool InRegWindow(uint32_t off) {
        return off >= kRegBase && off < kRegBase + kRegWindow;
    }
    static bool IsUsbHost(uint32_t roff) { return roff >= kUsbLo && roff <= kUsbHi; }
    static uint32_t SramOffset(uint32_t off) { return off >= kRegBase + kRegWindow ? off - kRegBase : off; }

    static constexpr uint32_t kPaletteEntries  = 256u;

    uint32_t Reg(uint32_t roff) const { return reg_[roff / 4u]; }
    void     ResetRegisters();
    uint32_t RegRead(uint32_t addr);
    void     RegWrite(uint32_t addr, uint32_t value);
    void     PublishScreenSizeOnEnableEdge();
    uint8_t* UsbBusMemory(uint32_t addr);
    void     RequireWordRegisterAccess(uint32_t off, uint32_t addr, uint32_t value);
    void     UpdateUsbClock();
    uint32_t InterruptStatus() const;
    uint32_t RawInterruptStatus();
    bool     GpioInputEnabled(uint32_t gpio) const;
    bool     GpioPinHigh(uint32_t gpio);
    bool     GpioEventTrue(uint32_t gpio);
    uint32_t ReadCc02R();
    void     LatchGpioStatus();
    void     WriteInterruptStatus(uint32_t value);
    void     UpdateInterruptPin(bool force);

    std::vector<uint8_t> sram_;
    uint32_t reg_[kRegWindow / 4u] = {};   /* whole 8 KB register window. */
    DisplayModeLatch mode_latch_;
    MediaQMq1188Ge ge_{*this};
    OhciHostController ohci_{emu_, kUsbProfile, *this};
    uint32_t in02_latched_ = 0;
    bool usb_interrupt_ = false;
    bool pin_high_      = false;
};

}

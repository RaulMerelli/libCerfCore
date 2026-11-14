#pragma once

#include "pci_bar.h"
#include "pci_common_header.h"

#include <cstddef>
#include <cstdint>

namespace cerfcore {

struct PciHeaderIdentity {
    PciCommonIdentity common;
    uint8_t           int_line_reset;
    uint8_t           int_line_writable;
    uint32_t          pin_gnt_lat;
};

class PciType0Header {
public:
    static constexpr size_t   kBars            = 6;
    static constexpr uint32_t kRegBarFirst     = 0x10u;
    static constexpr uint32_t kRegCardbusCis   = 0x28u;
    static constexpr uint32_t kRegSubsystem    = 0x2Cu;
    static constexpr uint32_t kRegExpansionRom = 0x30u;
    static constexpr uint32_t kRegCapPtr       = 0x34u;
    static constexpr uint32_t kRegInterrupt    = 0x3Cu;
    static constexpr uint32_t kRegDeviceFirst  = 0x40u;

    explicit PciType0Header(const PciHeaderIdentity& identity) : identity_(identity), common_(identity.common) {}

    void SetBarReset(size_t index, PciBar bar) { bar_reset_[index] = bar; }
    void Reset();

    bool Read(uint32_t reg, uint32_t* value) const;
    bool Write(uint32_t reg, uint32_t value);

    uint16_t      Command() const { return common_.Command(); }
    const PciBar& Bar(size_t index) const { return bars_[index]; }
    bool          Decodes(uint32_t addr, bool io) const;

    template <typename F>
    static constexpr void Visit(PciType0Header& h, F& field) {
        PciCommonHeader::Visit(h.common_, field);
        field("pci_int_line", h.int_line_);
        for (PciBar& b : h.bars_) PciBar::Visit(b, field);
    }

private:
    static bool IsBar(uint32_t reg) { return reg >= kRegBarFirst && reg < kRegBarFirst + 4u * kBars; }

    const PciHeaderIdentity& identity_;
    PciCommonHeader          common_;
    PciBar                   bar_reset_[kBars] = {};
    PciBar                   bars_[kBars]      = {};
    uint8_t                  int_line_         = 0;
};

}

#include "pci_type0_header.h"

namespace cerfcore {

void PciType0Header::Reset() {
    common_.Reset();
    int_line_ = identity_.int_line_reset;
    for (size_t i = 0; i < kBars; ++i) bars_[i] = bar_reset_[i];
}

bool PciType0Header::Read(uint32_t reg, uint32_t* value) const {
    if (common_.Read(reg, value)) return true;
    if (IsBar(reg)) {
        *value = bars_[(reg - kRegBarFirst) / 4u].Read();
    } else if (reg == kRegInterrupt) {
        *value = identity_.pin_gnt_lat | int_line_;
    } else {
        return false;
    }
    return true;
}

bool PciType0Header::Write(uint32_t reg, uint32_t value) {
    if (common_.Write(reg, value)) return true;
    if (IsBar(reg)) {
        bars_[(reg - kRegBarFirst) / 4u].Write(value);
    } else if (reg == kRegInterrupt) {
        int_line_ = PciCommonHeader::MergeWritable(int_line_, value, identity_.int_line_writable);
    } else {
        return false;
    }
    return true;
}

bool PciType0Header::Decodes(uint32_t addr, bool io) const {
    if ((common_.Command() & (io ? PciCommonHeader::kCommandIo : PciCommonHeader::kCommandMemory)) == 0u)
        return false;
    for (const PciBar& b : bars_)
        if (b.IsIo() == io && b.Decodes(addr)) return true;
    return false;
}

}

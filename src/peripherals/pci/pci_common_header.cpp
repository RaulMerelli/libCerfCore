#include "pci_common_header.h"

namespace cerfcore {

namespace {

constexpr uint32_t kHeaderTypeShift = 16u;
constexpr uint32_t kLatencyShift    = 8u;

}

void PciCommonHeader::Reset() {
    command_    = identity_.command_reset;
    cache_line_ = 0u;
    latency_    = 0u;
}

bool PciCommonHeader::Read(uint32_t reg, uint32_t* value) const {
    if (reg == kRegIdentity) {
        *value = identity_.id;
    } else if (reg == kRegCommand) {
        *value = command_ | (static_cast<uint32_t>(identity_.status) << 16);
    } else if (reg == kRegClass) {
        *value = identity_.class_rev;
    } else if (reg == kRegCacheLatency) {
        *value = cache_line_ | (static_cast<uint32_t>(latency_) << kLatencyShift) |
                 (static_cast<uint32_t>(identity_.header_type) << kHeaderTypeShift);
    } else {
        return false;
    }
    return true;
}

bool PciCommonHeader::Write(uint32_t reg, uint32_t value) {
    if (reg == kRegCommand) {
        command_ = MergeWritable(command_, value, identity_.command_writable);
    } else if (reg == kRegCacheLatency) {
        cache_line_ = MergeWritable(cache_line_, value, identity_.cache_line_writable);
        latency_    = MergeWritable(latency_, value >> kLatencyShift, identity_.latency_writable);
    } else {
        return false;
    }
    return true;
}

}

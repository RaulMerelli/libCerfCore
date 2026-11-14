#pragma once

#include <bit>
#include <cstdint>

namespace cerfcore {

struct Vrc5477Pdar {
    static constexpr uint32_t kAddrMask         = 0xFFE00000u;
    static constexpr uint32_t kMaskBits         = 0x0000000Fu;
    static constexpr uint32_t kOnMemoryBus      = 0x00000010u;
    static constexpr uint32_t kPciVisible       = 0x00000020u;
    static constexpr uint32_t kWidthMask        = 0x000000C0u;
    static constexpr uint32_t kWidth32          = 0x00000080u;
    static constexpr uint32_t kLowBits          = 0x000000FFu;
    static constexpr uint32_t kSmallestMaskBits = 5u;
    static constexpr uint32_t kLargestMaskBits  = 15u;
    static constexpr uint32_t kSizeShiftBase    = 36u;

    uint32_t value;

    static constexpr Vrc5477Pdar Window32(uint32_t phys, uint32_t size, bool pci_visible) {
        return {(kSizeShiftBase - static_cast<uint32_t>(std::countr_zero(size))) | (pci_visible ? kPciVisible : 0u) |
                kWidth32 | (phys & kAddrMask)};
    }

    uint32_t MaskBits() const { return value & kMaskBits; }
    bool     Enabled() const { return MaskBits() >= kSmallestMaskBits; }
    bool     PciVisible() const { return (value & kPciVisible) != 0u; }
    bool     OnMemoryBus() const { return (value & kOnMemoryBus) != 0u; }
    bool     Width32() const { return (value & kWidthMask) == kWidth32; }
    bool     StrayBits() const { return (value & ~(kAddrMask | kLowBits)) != 0u; }
    uint32_t Base() const { return value & kAddrMask; }
    uint32_t Size() const { return 1u << (kSizeShiftBase - MaskBits()); }
};

}

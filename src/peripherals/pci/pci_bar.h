#pragma once

#include <cstdint>

namespace cerfcore {

struct PciBar {
    static constexpr uint32_t kIoSpace = 0x1u;

    uint32_t size_mask = 0;
    uint32_t flags     = 0;
    uint32_t base      = 0;

    uint32_t Read() const { return size_mask ? (base | flags) : 0u; }
    void Write(uint32_t v) {
        if (size_mask) base = v & size_mask;
    }

    bool IsIo() const { return (flags & kIoSpace) != 0u; }
    bool Decodes(uint32_t addr) const { return size_mask != 0u && addr - base <= ~size_mask; }

    template <typename F>
    static constexpr void Visit(PciBar& b, F& field) {
        field("bar_size_mask", b.size_mask);
        field("bar_flags", b.flags);
        field("bar_base", b.base);
    }
};

}

#pragma once

#include <cstdint>

namespace cerfcore {

struct PciCommonIdentity {
    uint32_t id;
    uint32_t class_rev;
    uint16_t status;
    uint16_t command_reset;
    uint16_t command_writable;
    uint8_t  cache_line_writable;
    uint8_t  latency_writable;
    uint8_t  header_type;
};

class PciCommonHeader {
public:
    static constexpr uint32_t kRegIdentity      = 0x00u;
    static constexpr uint32_t kRegCommand       = 0x04u;
    static constexpr uint32_t kRegClass         = 0x08u;
    static constexpr uint32_t kRegCacheLatency  = 0x0Cu;
    static constexpr uint16_t kCommandIo        = 0x0001u;
    static constexpr uint16_t kCommandMemory    = 0x0002u;
    static constexpr uint16_t kCommandBusMaster = 0x0004u;

    explicit PciCommonHeader(const PciCommonIdentity& identity) : identity_(identity) {}

    void Reset();

    bool Read(uint32_t reg, uint32_t* value) const;
    bool Write(uint32_t reg, uint32_t value);

    uint16_t Command() const { return command_; }

    template <typename T>
    static constexpr T MergeWritable(T current, uint32_t value, T writable) {
        return static_cast<T>((current & ~writable) | (value & writable));
    }

    template <typename F>
    static constexpr void Visit(PciCommonHeader& h, F& field) {
        field("pci_command", h.command_);
        field("pci_cache_line", h.cache_line_);
        field("pci_latency", h.latency_);
    }

private:
    const PciCommonIdentity& identity_;
    uint16_t                 command_    = 0;
    uint8_t                  cache_line_ = 0;
    uint8_t                  latency_    = 0;
};

}

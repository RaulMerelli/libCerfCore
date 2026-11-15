#pragma once

#include "machine_object.h"
#include "published_state.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

struct ChannelCounters {
    uint64_t received    = 0;
    uint64_t sent        = 0;
    uint64_t accepted    = 0;
    uint64_t unsupported = 0;
    uint64_t invalid     = 0;
    uint64_t unavailable = 0;
    uint64_t malformed   = 0;
};

struct ChannelSignal {
    uint32_t id       = 0;
    bool     reported = false;
    uint64_t value    = 0;

    bool operator==(const ChannelSignal&) const = default;
};

class MachineDiagnosticsChannel : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kDiagnosticsChannel; }

    virtual ChannelCounters            Counters() const = 0;
    virtual std::vector<ChannelSignal> Signals() const  = 0;

protected:
    void RefreshSignals();

private:
    void Republish() override { RefreshSignals(); }

    PublishedState<std::vector<ChannelSignal>> published_;
};

}

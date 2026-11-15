#pragma once

#include "../../execution/guest_cycle_clock.h"

#include <cerfcore/host/machine_socket.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace cerfcore {

class CerfEmulator;
class PcmciaCard;
class StateReader;
class StateWriter;

class PcmciaSwapGap {
public:
    using Seat = std::function<void(std::unique_ptr<PcmciaCard>)>;

    PcmciaSwapGap(CerfEmulator& emu, Seat seat);
    ~PcmciaSwapGap();

    void                        Open();
    bool                        Hold(std::unique_ptr<PcmciaCard>& card, bool& replaced);
    bool                        Drop();
    std::optional<InsertedItem> HeldItem() const;

    void Save(StateWriter& w);
    void Restore(StateReader& r);
    void PostRestore();

private:
    void                    PostStart(uint64_t seq);
    void                    Start(uint64_t seq);
    void                    ArmAtDeadline();
    void                    Close();
    GuestCycleClock::Event* Event();

    CerfEmulator&               emu_;
    Seat                        seat_;
    mutable std::mutex          mtx_;
    bool                        open_        = false;
    bool                        started_     = false;
    uint64_t                    seq_         = 0;
    uint64_t                    armed_seq_   = 0;
    int64_t                     deadline_ns_ = 0;
    int64_t                     left_ns_     = 0;
    std::unique_ptr<PcmciaCard> held_;
    GuestCycleClock::Event*     event_ = nullptr;
};

}

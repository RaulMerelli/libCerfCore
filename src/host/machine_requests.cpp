#include <cerfcore/host/machine_requests.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>

#include <algorithm>
#include <utility>

namespace cerfcore {

REGISTER_SERVICE(MachineRequests);

void MachineRequests::Ask(MachineRequest request, Answer answer) {
    std::vector<Listener> listeners;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        request.serial = next_serial_++;
        pending_.push_back(Pending{request, std::move(answer)});
        listeners = listeners_;
    }
    for (const Listener& listener : listeners) listener(request);
}

bool MachineRequests::Fits(const MachineRequest& request, const MachineReply& reply) {
    if (reply.cancelled) return true;
    switch (request.kind) {
        case MachineRequestKind::kPath:      return !reply.path.empty();
        case MachineRequestKind::kDocuments: return !reply.documents.empty();
        case MachineRequestKind::kSize:      return reply.size >= request.minimum && reply.size <= request.maximum;
    }
    return false;
}

bool MachineRequests::Reply(uint64_t serial, const MachineReply& reply) {
    Answer answer;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = std::find_if(pending_.begin(), pending_.end(),
                                     [serial](const Pending& p) { return p.request.serial == serial; });
        if (it == pending_.end()) {
            LOG(Machine, "[MACHINE] refused reply to unknown request %llu\n", static_cast<unsigned long long>(serial));
            return false;
        }
        if (!Fits(it->request, reply)) {
            LOG(Machine, "[MACHINE] refused reply to request %llu (%s): outside its limits\n",
                static_cast<unsigned long long>(serial), it->request.id.c_str());
            return false;
        }
        answer = std::move(it->answer);
        pending_.erase(it);
    }
    answer(reply);
    return true;
}

void MachineRequests::Subscribe(Listener listener) {
    std::lock_guard<std::mutex> lk(mtx_);
    listeners_.push_back(std::move(listener));
}

}

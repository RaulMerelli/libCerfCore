#include <cerfcore/host/machine_socket.h>

#include <cerfcore/core/log.h>
#include <cerfcore/host/machine_object_tree.h>

#include <algorithm>

namespace cerfcore {

namespace {

InsertStatus Check(const std::vector<InsertableType>& accepted, const std::string& type_id,
                   const std::string& command_id, const std::string& argument) {
    const auto type = std::find_if(accepted.begin(), accepted.end(),
                                   [&](const InsertableType& t) { return t.id == type_id; });
    if (type == accepted.end()) return InsertStatus::kUnknownType;
    const auto command = std::find_if(type->commands.begin(), type->commands.end(),
                                      [&](const InsertCommand& c) { return c.id == command_id; });
    if (command == type->commands.end()) return InsertStatus::kUnknownCommand;
    if (command->arguments.empty()) return argument.empty() ? InsertStatus::kAccepted : InsertStatus::kInvalidArgument;
    const bool listed = std::any_of(command->arguments.begin(), command->arguments.end(),
                                    [&](const InsertArgument& a) { return a.id == argument; });
    return listed ? InsertStatus::kAccepted : InsertStatus::kInvalidArgument;
}

}

InsertStatus MachineSocket::Insert(const std::string& type_id, const std::string& command_id,
                                   const std::string& argument) {
    const InsertStatus status = Check(Accepted(), type_id, command_id, argument);
    if (status != InsertStatus::kAccepted) {
        LOG(Machine, "[MACHINE] %s refused insert type=%s command=%s argument=%s status=%u\n",
            ObjectId().c_str(), type_id.c_str(), command_id.c_str(), argument.c_str(),
            static_cast<unsigned>(status));
        return status;
    }
    InsertAccepted(type_id, command_id, argument);
    return status;
}

void MachineSocket::ReportInserted() {
    if (MachineObjectTree* tree = tree_.load(std::memory_order_acquire)) tree->NotifyInserted(this);
}

void MachineSocket::ReportEjected() {
    if (MachineObjectTree* tree = tree_.load(std::memory_order_acquire)) tree->NotifyEjected(this);
}

}

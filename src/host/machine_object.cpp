#include <cerfcore/host/machine_object.h>

#include <cerfcore/host/machine_object_tree.h>

#include <utility>

namespace cerfcore {

void MachineObject::Changed() {
    if (MachineObjectTree* tree = tree_.load(std::memory_order_acquire)) tree->NotifyChanged(this);
}

void MachineObject::Failed(std::string reason, int64_t code, std::string subject) {
    if (MachineObjectTree* tree = tree_.load(std::memory_order_acquire))
        tree->NotifyFailed(this, std::move(reason), code, std::move(subject));
}

}

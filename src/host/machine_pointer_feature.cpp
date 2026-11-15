#include <cerfcore/host/machine_pointer_feature.h>

namespace cerfcore {

void MachinePointerFeature::Refresh() {
    if (published_.Replace([this] { return Published{Available(), Enabled()}; })) Changed();
}

}

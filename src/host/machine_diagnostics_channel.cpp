#include <cerfcore/host/machine_diagnostics_channel.h>

namespace cerfcore {

void MachineDiagnosticsChannel::RefreshSignals() {
    if (published_.Replace([this] { return Signals(); })) Changed();
}

}

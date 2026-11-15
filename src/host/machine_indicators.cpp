#include <cerfcore/host/machine_indicators.h>

namespace cerfcore {

void MachineLamp::Refresh() {
    if (published_.Replace([this] { return Lamp(); })) Changed();
}

void MachineVibrator::Refresh() {
    if (published_.Replace([this] { return Vibrating(); })) Changed();
}

void MachineCharacterDisplay::Refresh() {
    if (published_.Replace([this] { return Characters(); })) Changed();
}

}

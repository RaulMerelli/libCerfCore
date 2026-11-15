#include "audio_activity.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>

namespace cerfcore {

void AudioActivity::NotePresent() {
    if (added_) return;
    added_ = true;
    emu_.Get<MachineObjectTree>().Add(this);
}

REGISTER_SERVICE(AudioActivity);

}

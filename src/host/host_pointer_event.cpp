#include <cerfcore/host/host_pointer_event.h>

namespace cerfcore {

uint32_t HostButtonBit(HostButton button) {
    switch (button) {
        case HostButton::kLeft:   return kHeldLeft;
        case HostButton::kRight:  return kHeldRight;
        case HostButton::kMiddle: return kHeldMiddle;
        case HostButton::kNone:   return 0;
    }
    return 0;
}

}

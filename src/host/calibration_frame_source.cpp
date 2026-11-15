#include "calibration_frame_source.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/frame_renderer.h>
#include <cerfcore/host/host_presentation.h>

#include <cstddef>
#include <cstdint>

namespace cerfcore {

REGISTER_SERVICE(CalibrationFrameSource);

void CalibrationFrameSource::OnReady() {
    renderer_     = &emu_.Get<FrameRenderer>();
    presentation_ = &emu_.Get<HostPresentation>();
}

bool CalibrationFrameSource::Grab(CalibrationFrame& frame) {
    if (!renderer_->HasFrame()) return false;
    const uint32_t w = presentation_->SurfaceWidth();
    const uint32_t h = presentation_->SurfaceHeight();
    if (w == 0 || h == 0) return false;
    frame.pixels.resize(static_cast<size_t>(w) * h);
    renderer_->RenderInto(frame.pixels.data(), w, h);
    frame.width  = w;
    frame.height = h;
    return true;
}

}

#define NOMINMAX

#include "sa11xx_dma_capture.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/cpu/emulated_memory.h>
#include "../../host/audio_activity.h"
#include "sa11xx_dma.h"

#include <vector>

namespace cerfcore {

void Sa11xxDmaCapture::OnReady() {
    cfg_ = AudioConfig();
    mic_.Start(emu_.Get<InstanceThreads>(), emu_.TryGet<AudioInputs>(), cfg_.log_tag, static_cast<size_t>(cfg_.max_page_bytes) * 4u);
    emu_.Get<Sa11xxDma>().RegisterReceiveSource(this);
    emu_.Get<AudioActivity>().NotePresent();
}

void Sa11xxDmaCapture::OnShutdown() { mic_.Stop(); }

bool Sa11xxDmaCapture::FillReceived(uint32_t ddar, uint32_t pa, uint32_t bytes,
                                    GuestCycleClock::Rate word_rate) {
    if ((ddar & cfg_.ddar_mask) != cfg_.ddar_value) return false;
    const uint32_t rate = static_cast<uint32_t>(word_rate.Scaled(1u, cfg_.channels).RoundedHz());
    mic_.Request(rate, cfg_.channels, cfg_.bits_per_sample);
    std::vector<uint8_t> data(bytes, 0u);
    const size_t taken = mic_.Take(data.data(), bytes);
    emu_.Get<EmulatedMemory>().CopyIn(pa, data.data(), bytes);
    if (taken != 0u) emu_.Get<AudioActivity>().MarkRx();
    return true;
}

void Sa11xxDmaCapture::OnReceiveRestored() { mic_.Clear(); }

}

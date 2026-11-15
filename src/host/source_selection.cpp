#include <cerfcore/host/source_selection.h>

#include <cerfcore/core/utf16.h>
#include <cerfcore/host/keyboard_input.h>
#include <cerfcore/host/pointer_source.h>
#include <cerfcore/state/state_stream.h>

#include <cstdint>
#include <string>

namespace cerfcore {

template <typename Source>
template <typename Mutate>
void SourceSelection<Source>::Update(Mutate mutate) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(selection_mtx_);
        const Source* before = active_;
        const size_t  count  = sources_.size();
        mutate();
        changed = active_ != before || sources_.size() != count;
    }
    if (changed) SelectionChanged();
}

template <typename Source>
void SourceSelection<Source>::Register(Source* src) {
    Update([&] {
        for (Source* s : sources_)
            if (s == src) return;
        sources_.push_back(src);
        if (!user_picked_) SelectAutoLocked();
    });
}

template <typename Source>
std::vector<Source*> SourceSelection<Source>::Sources() const {
    std::lock_guard<std::mutex> lk(selection_mtx_);
    return sources_;
}

template <typename Source>
Source* SourceSelection<Source>::Active() const {
    std::lock_guard<std::mutex> lk(selection_mtx_);
    return active_;
}

template <typename Source>
void SourceSelection<Source>::SetActive(Source* src) {
    Update([&] {
        for (Source* s : sources_) {
            if (s != src) continue;
            active_      = src;
            user_picked_ = true;
            return;
        }
    });
}

template <typename Source>
void SourceSelection<Source>::CycleNext() {
    Update([&] {
        if (sources_.size() < 2) return;
        user_picked_ = true;
        for (size_t i = 0; i < sources_.size(); ++i) {
            if (sources_[i] == active_) {
                active_ = sources_[(i + 1) % sources_.size()];
                return;
            }
        }
        active_ = sources_.front();
    });
}

template <typename Source>
void SourceSelection<Source>::ReevaluateAuto() {
    Update([&] {
        if (!user_picked_) SelectAutoLocked();
    });
}

template <typename Source>
void SourceSelection<Source>::RearmAutoSelect() {
    Update([&] {
        user_picked_ = false;
        SelectAutoLocked();
    });
}

template <typename Source>
void SourceSelection<Source>::SelectAutoLocked() {
    Source* best_ready = nullptr;
    Source* best_any   = nullptr;
    for (Source* s : sources_) {
        if (!best_any || s->SourcePriority() > best_any->SourcePriority()) best_any = s;
        if (!s->SourceReady()) continue;
        if (!best_ready || s->SourcePriority() > best_ready->SourcePriority()) best_ready = s;
    }
    active_ = best_ready ? best_ready : best_any;
}

template <typename Source>
void SourceSelection<Source>::SaveSelection(StateWriter& w) const {
    std::u16string id;
    bool           picked = false;
    {
        std::lock_guard<std::mutex> lk(selection_mtx_);
        if (active_) id = Utf8ToUtf16(active_->SourceId());
        picked = user_picked_;
    }
    w.Write<uint32_t>("source_id_count", static_cast<uint32_t>(id.size()));
    w.WriteBytes("source_id", id.data(), id.size() * sizeof(char16_t));
    w.Write<uint8_t>("user_picked", picked ? 1u : 0u);
}

template <typename Source>
void SourceSelection<Source>::RestoreSelection(StateReader& r) {
    uint32_t n = 0;
    r.Read("source_id_count", n);
    std::u16string units(n, u'\0');
    r.ReadBytes("source_id", units.data(), n * sizeof(char16_t));
    uint8_t picked = 0;
    r.Read("user_picked", picked);
    const std::string id = Utf16ToUtf8(units);
    Update([&] {
        for (Source* s : sources_) {
            if (s->SourceId() != id) continue;
            active_ = s;
            break;
        }
        user_picked_ = picked != 0;
    });
}

template class SourceSelection<KeyboardInput>;
template class SourceSelection<PointerSource>;

}

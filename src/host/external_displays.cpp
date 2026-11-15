#include <cerfcore/host/external_displays.h>

#include <cerfcore/core/cerf_emulator.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(ExternalDisplays);

int ExternalDisplays::Attach(FrameSource& source, std::string name, uint32_t width, uint32_t height,
                             std::function<void()> eject) {
    ExternalDisplayInfo info;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        info = ExternalDisplayInfo{next_id_++, std::move(name), width, height};
        entries_.emplace(info.id, Entry{&source, info, std::move(eject)});
    }
    Publish(ExternalDisplayEvent{ExternalDisplayEventKind::kAttached, info});
    return info.id;
}

void ExternalDisplays::Resize(int id, uint32_t width, uint32_t height) {
    ExternalDisplayInfo info;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = entries_.find(id);
        if (it == entries_.end()) return;
        it->second.info.width  = width;
        it->second.info.height = height;
        info                   = it->second.info;
    }
    Publish(ExternalDisplayEvent{ExternalDisplayEventKind::kResized, info});
}

void ExternalDisplays::Detach(int id) {
    ExternalDisplayInfo info;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = entries_.find(id);
        if (it == entries_.end()) return;
        info = it->second.info;
        entries_.erase(it);
    }
    Publish(ExternalDisplayEvent{ExternalDisplayEventKind::kDetached, info});
}

std::vector<ExternalDisplayInfo> ExternalDisplays::Displays() {
    std::vector<ExternalDisplayInfo> out;
    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& [id, entry] : entries_) out.push_back(entry.info);
    return out;
}

bool ExternalDisplays::UseSource(int id, const std::function<void(FrameSource&)>& use) {
    std::lock_guard<std::mutex> lk(mtx_);
    const auto it = entries_.find(id);
    if (it == entries_.end()) return false;
    use(*it->second.source);
    return true;
}

void ExternalDisplays::Eject(int id) {
    std::function<void()> eject;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = entries_.find(id);
        if (it == entries_.end()) return;
        eject = it->second.eject;
    }
    eject();
}

void ExternalDisplays::Subscribe(Listener listener) {
    std::lock_guard<std::mutex> lk(listeners_mtx_);
    listeners_.push_back(std::move(listener));
}

void ExternalDisplays::Publish(const ExternalDisplayEvent& event) {
    std::vector<Listener> listeners;
    {
        std::lock_guard<std::mutex> lk(listeners_mtx_);
        listeners = listeners_;
    }
    for (const Listener& listener : listeners) listener(event);
}

}

#include <cerfcore/host/hw_screen.h>

#include <cerfcore/core/cerf_emulator.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(HwScreen);

void HwScreen::AddLine(std::string_view line) {
    std::string added(line);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (lines_.size() >= kMaxLines) lines_.pop_front();
        lines_.push_back(added);
    }
    std::vector<LineListener> listeners;
    {
        std::lock_guard<std::mutex> lk(listeners_mtx_);
        listeners = listeners_;
    }
    for (const LineListener& listener : listeners) listener(added);
}

void HwScreen::Subscribe(LineListener listener) {
    std::lock_guard<std::mutex> lk(listeners_mtx_);
    listeners_.push_back(std::move(listener));
}

std::string HwScreen::LastLine() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return lines_.empty() ? std::string() : lines_.back();
}

void HwScreen::Snapshot(std::vector<std::string>& out) const {
    std::lock_guard<std::mutex> lk(mtx_);
    out.assign(lines_.begin(), lines_.end());
}

}

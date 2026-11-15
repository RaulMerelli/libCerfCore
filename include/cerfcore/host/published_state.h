#pragma once

#include <mutex>
#include <optional>
#include <utility>

namespace cerfcore {

template <class State>
class PublishedState {
public:
    template <class Read>
    bool Replace(Read read) {
        std::lock_guard<std::mutex> lk(mtx_);
        State now = read();
        if (published_ && *published_ == now) return false;
        published_ = std::move(now);
        return true;
    }

private:
    std::mutex           mtx_;
    std::optional<State> published_;
};

}

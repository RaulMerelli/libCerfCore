#pragma once

#include "../core/service.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace cerfcore {

class HwScreen : public Service {
public:
    using Service::Service;
    using LineListener = std::function<void(const std::string& line)>;

    void AddLine(std::string_view line);
    void Subscribe(LineListener listener);

    std::string LastLine() const;
    void        Snapshot(std::vector<std::string>& out) const;

private:
    static constexpr size_t kMaxLines = 500;

    mutable std::mutex        mtx_;
    std::deque<std::string>   lines_;
    std::mutex                listeners_mtx_;
    std::vector<LineListener> listeners_;
};

}

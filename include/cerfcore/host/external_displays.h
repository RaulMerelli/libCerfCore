#pragma once

#include "../core/service.h"

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace cerfcore {

class FrameSource;

enum class ExternalDisplayEventKind : uint8_t { kAttached, kResized, kDetached };

struct ExternalDisplayInfo {
    int         id = 0;
    std::string name;
    uint32_t    width  = 0;
    uint32_t    height = 0;
};

struct ExternalDisplayEvent {
    ExternalDisplayEventKind kind = ExternalDisplayEventKind::kAttached;
    ExternalDisplayInfo      display;
};

class ExternalDisplays : public Service {
public:
    using Service::Service;
    using Listener = std::function<void(const ExternalDisplayEvent&)>;

    int  Attach(FrameSource& source, std::string name, uint32_t width, uint32_t height, std::function<void()> eject);
    void Resize(int id, uint32_t width, uint32_t height);
    void Detach(int id);

    std::vector<ExternalDisplayInfo> Displays();
    bool UseSource(int id, const std::function<void(FrameSource&)>& use);
    void Eject(int id);
    void Subscribe(Listener listener);

private:
    struct Entry {
        FrameSource*          source = nullptr;
        ExternalDisplayInfo   info;
        std::function<void()> eject;
    };

    void Publish(const ExternalDisplayEvent& event);

    std::mutex            mtx_;
    std::map<int, Entry>  entries_;
    int                   next_id_ = 1;
    std::mutex            listeners_mtx_;
    std::vector<Listener> listeners_;
};

}

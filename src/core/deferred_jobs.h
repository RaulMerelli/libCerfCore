#pragma once

#include <cerfcore/core/service.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace cerfengine { class HostRequestChannel; }

namespace cerfcore {

class DeferredJobs : public Service {
public:
    using Service::Service;

    void OnReady() override;
    void OnShutdown() override;

    void Post(std::function<void()> job);
    void PostToGuestThread(std::function<void()> job);

private:
    void Start();
    void Run();
    void RunGuestJobs();

    cerfengine::HostRequestChannel*    requests_ = nullptr;
    std::mutex                         mtx_;
    std::condition_variable            wake_;
    std::vector<std::function<void()>> host_jobs_;
    std::vector<std::function<void()>> guest_jobs_;
    bool                               stop_ = false;
    std::thread                        worker_;
};

}

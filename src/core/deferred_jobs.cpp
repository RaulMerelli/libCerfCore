#include "deferred_jobs.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/execution/jit_runner.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfengine/host_request_channel.h>
#include "../execution/engine_bridge.h"

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(DeferredJobs);

void DeferredJobs::OnReady() {
    requests_ = &emu_.Get<EngineBridge>().Engine().Get<cerfengine::HostRequestChannel>();
    requests_->RegisterListener([this] { RunGuestJobs(); });
    emu_.Get<GuestDeepSleep>().RegisterParkClock([this] { RunGuestJobs(); });
    emu_.Get<JitRunner>().RegisterPreStartHook([this] { Start(); });
}

void DeferredJobs::Start() {
    worker_ = emu_.Get<InstanceThreads>().Start([this] { Run(); });
}

void DeferredJobs::OnShutdown() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
        host_jobs_.clear();
        guest_jobs_.clear();
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void DeferredJobs::Post(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (stop_) return;
        host_jobs_.push_back(std::move(job));
    }
    wake_.notify_one();
}

void DeferredJobs::PostToGuestThread(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (stop_) return;
        guest_jobs_.push_back(std::move(job));
    }
    requests_->Request();
}

void DeferredJobs::Run() {
    for (;;) {
        std::vector<std::function<void()>> jobs;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            wake_.wait(lk, [this] { return stop_ || !host_jobs_.empty(); });
            if (stop_) return;
            jobs.swap(host_jobs_);
        }
        for (auto& job : jobs) job();
    }
}

void DeferredJobs::RunGuestJobs() {
    std::vector<std::function<void()>> jobs;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        jobs.swap(guest_jobs_);
    }
    for (auto& job : jobs) job();
}

}

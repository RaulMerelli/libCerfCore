#pragma once

#include "../core/callback_list.h"
#include "../core/service.h"

#include <functional>

namespace cerfcore {

class StateBootGate : public Service {
public:
    using Service::Service;
    void OnReady() override;

    void RegisterColdStartListener(std::function<void()> fn);

private:
    void Run();

    CallbackList<> cold_start_listeners_;
};

}

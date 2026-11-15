#pragma once

#include "service.h"
#include "callback_list.h"
#include "device_config.h"

#include <functional>

namespace cerfcore {

class DeviceConfigRefresh : public Service {
public:
    using Service::Service;

    void RegisterListener(std::function<void()> fn);

    void Refresh();

private:
    LiveDeviceSettings LoadFiles(const DeviceConfig& config);

    CallbackList<> listeners_;
};

}

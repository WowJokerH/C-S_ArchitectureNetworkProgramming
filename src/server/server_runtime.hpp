#pragma once

#include <atomic>

struct ServerRuntimeConfig {
    std::atomic<bool> intervalControl{false};
    std::atomic<int> forcedIntervalMs{3000};
};

#pragma once
#include <atomic>

struct SharedParams {
    std::atomic<float> centerX{0.0f};
    std::atomic<float> centerY{0.0f};
    std::atomic<float> centerZ{0.0f};
    std::atomic<float> rotX{0.0f};
    std::atomic<float> rotY{0.0f};
    std::atomic<float> rotZ{0.0f};
    std::atomic<bool> valid{false};
};

extern SharedParams g_sharedParams;

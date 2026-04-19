#pragma once

#include <chrono>

class FrameTimer {
public:
    FrameTimer() : m_lastTick(std::chrono::high_resolution_clock::now()) {}

    float tick() {
        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - m_lastTick).count();
        m_lastTick = now;
        return deltaTime;
    }

    void reset() {
        m_lastTick = std::chrono::high_resolution_clock::now();
    }

private:
    std::chrono::high_resolution_clock::time_point m_lastTick;
};

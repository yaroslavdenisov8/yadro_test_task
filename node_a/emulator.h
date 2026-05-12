#pragma once

#include <chrono>
#include <accel.pb.h>

class AccelEmulator {
public:
    static constexpr float kAmpX  = 0.5f;
    static constexpr float kFreqX = 0.8f;

    static constexpr float kAmpY  = 0.3f;
    static constexpr float kFreqY = 1.2f;

    static constexpr float kAmpZ  = 0.2f;
    static constexpr float kFreqZ = 0.5f;

    AccelEmulator(double freq) :
        _period_ms(1e3 / freq),
        _start_time(std::chrono::system_clock::now()) {}
    AccelPacket generate();
    void wait_for_next_sample();

private:
    double _period_ms;
    std::chrono::system_clock::time_point _start_time;
};

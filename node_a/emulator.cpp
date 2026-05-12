#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <string>

#include "emulator.h"

AccelPacket AccelEmulator::generate() {
    auto now = std::chrono::system_clock::now();
    double t_sec = std::chrono::duration<double>(now - _start_time).count();

    int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    float x = kAmpX * std::sin(2.0 * M_PI * kFreqX * t_sec);
    float y = kAmpY * std::sin(2.0 * M_PI * kFreqY * t_sec);
    float z = kAmpZ * std::sin(2.0 * M_PI * kFreqZ * t_sec);

    AccelPacket pkt;
    pkt.set_timestamp(ts_ms);
    pkt.set_x(x);
    pkt.set_y(y);
    pkt.set_z(z);

    return pkt;
}

void AccelEmulator::wait_for_next_sample() {
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(_period_ms)));
}

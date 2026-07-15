/*
 * Copyright (c) 2026, The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

#include "thermal.h"

using aidl::android::hardware::thermal::CoolingDevice;
using aidl::android::hardware::thermal::Temperature;
using aidl::android::hardware::thermal::Thermal;

namespace {

constexpr auto kInitializationTimeout = std::chrono::seconds(5);
constexpr auto kPollInterval = std::chrono::milliseconds(100);

[[noreturn]] void finish(int status) {
    std::fflush(nullptr);
    std::_Exit(status);
}

}  // namespace

int main() {
    auto thermal = ndk::SharedRefBase::make<Thermal>();
    thermal->startInitialization();

    std::vector<Temperature> temperatures;
    ndk::ScopedAStatus temperature_status;
    const auto deadline = std::chrono::steady_clock::now() + kInitializationTimeout;
    do {
        temperatures.clear();
        temperature_status = thermal->getTemperatures(&temperatures);
        if (temperature_status.isOk() && !temperatures.empty()) {
            break;
        }
        std::this_thread::sleep_for(kPollInterval);
    } while (std::chrono::steady_clock::now() < deadline);

    if (!temperature_status.isOk() || temperatures.empty()) {
        std::fprintf(stderr, "thermal_selftest: no readable temperatures: %s\n",
                     temperature_status.getDescription().c_str());
        finish(2);
    }

    std::printf("temperature_count=%zu\n", temperatures.size());
    for (const auto& temperature : temperatures) {
        std::printf("temperature name=%s type=%d value=%.3f status=%d\n",
                    temperature.name.c_str(), static_cast<int>(temperature.type),
                    temperature.value,
                    static_cast<int>(temperature.throttlingStatus));
    }

    std::vector<CoolingDevice> cooling_devices;
    const auto cooling_status = thermal->getCoolingDevices(&cooling_devices);
    if (!cooling_status.isOk()) {
        std::fprintf(stderr, "cooling_devices=unavailable status=%s\n",
                     cooling_status.getDescription().c_str());
        finish(3);
    }

    std::printf("cooling_device_count=%zu\n", cooling_devices.size());
    for (const auto& cooling_device : cooling_devices) {
        std::printf("cooling_device name=%s type=%d value=%lld\n",
                    cooling_device.name.c_str(), static_cast<int>(cooling_device.type),
                    static_cast<long long>(cooling_device.value));
    }

    finish(0);
}

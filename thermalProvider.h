/*
 * Copyright (c) 2026, The LineageOS Project
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef ANDROID_QTI_THERMAL_PROVIDER_H
#define ANDROID_QTI_THERMAL_PROVIDER_H

#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace aidl::android::hardware::thermal {

template <typename Provider>
class AsyncProvider {
  public:
	using Factory = std::function<std::shared_ptr<Provider>()>;

	bool start(Factory factory) noexcept {
		auto state = state_;
		try {
			std::thread([state, factory = std::move(factory)]() mutable {
				try {
					auto provider = factory();
					if (provider == nullptr) {
						return;
					}
					std::lock_guard<std::mutex> lock(state->mutex);
					state->provider = std::move(provider);
				} catch (...) {
					// Initialization failure leaves the provider unavailable.
				}
			}).detach();
			return true;
		} catch (...) {
			return false;
		}
	}

	std::shared_ptr<Provider> get() const {
		std::lock_guard<std::mutex> lock(state_->mutex);
		return state_->provider;
	}

  private:
	struct State {
		std::mutex mutex;
		std::shared_ptr<Provider> provider;
	};

	std::shared_ptr<State> state_ = std::make_shared<State>();
};

}  // namespace aidl::android::hardware::thermal

#endif  // ANDROID_QTI_THERMAL_PROVIDER_H

// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "../thermalProvider.h"

#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {

using namespace std::chrono_literals;
using aidl::android::hardware::thermal::AsyncProvider;

struct Provider {
	explicit Provider(int value) : value(value) {}
	int value;
};

void require(bool condition, const char* message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(EXIT_FAILURE);
	}
}

template <typename Predicate>
bool waitFor(Predicate predicate) {
	const auto deadline = std::chrono::steady_clock::now() + 500ms;
	do {
		if (predicate()) {
			return true;
		}
		std::this_thread::sleep_for(1ms);
	} while (std::chrono::steady_clock::now() < deadline);
	return false;
}

void hungInitializationDoesNotBlockStartup() {
	AsyncProvider<Provider> provider;
	std::promise<void> entered;
	auto entered_future = entered.get_future();
	std::promise<void> release;
	auto release_future = release.get_future().share();

	const auto before = std::chrono::steady_clock::now();
	const bool started = provider.start([&entered, release_future]() {
		entered.set_value();
		release_future.wait();
		return std::make_shared<Provider>(1);
	});
	const auto elapsed = std::chrono::steady_clock::now() - before;

	require(started, "startup could not launch the provider factory");
	require(elapsed < 250ms, "startup waited for a hung provider factory");
	require(entered_future.wait_for(500ms) == std::future_status::ready,
			"hung provider factory did not start");
	require(provider.get() == nullptr,
			"provider became readable before initialization completed");

	release.set_value();
	require(waitFor([&provider]() { return provider.get() != nullptr; }),
			"released provider did not become available");
}

void failedInitializationRemainsUnavailable() {
	AsyncProvider<Provider> provider;
	std::promise<void> attempted;
	auto attempted_future = attempted.get_future();

	const bool started = provider.start([&attempted]() -> std::shared_ptr<Provider> {
		attempted.set_value();
		return nullptr;
	});

	require(started, "startup could not launch the failing provider factory");
	require(attempted_future.wait_for(500ms) == std::future_status::ready,
			"failed provider factory did not run");
	require(provider.get() == nullptr,
			"failed provider initialization published data");
}

void throwingInitializationRemainsUnavailable() {
	AsyncProvider<Provider> provider;
	std::promise<void> attempted;
	auto attempted_future = attempted.get_future();

	const bool started = provider.start([&attempted]() -> std::shared_ptr<Provider> {
		attempted.set_value();
		throw std::runtime_error("sensor initialization failed");
	});

	require(started, "startup could not launch the throwing provider factory");
	require(attempted_future.wait_for(500ms) == std::future_status::ready,
			"throwing provider factory did not run");
	require(provider.get() == nullptr,
			"throwing provider initialization published data");
}

void successfulInitializationPublishesProvider() {
	AsyncProvider<Provider> provider;
	provider.start([]() { return std::make_shared<Provider>(42); });

	require(waitFor([&provider]() { return provider.get() != nullptr; }),
			"successful provider initialization was not published");
	require(provider.get()->value == 42,
			"published provider changed the initialized value");
}

}  // namespace

int main() {
	hungInitializationDoesNotBlockStartup();
	failedInitializationRemainsUnavailable();
	throwingInitializationRemainsUnavailable();
	successfulInitializationPublishesProvider();
	std::cout << "async provider: PASS\n";
	return EXIT_SUCCESS;
}

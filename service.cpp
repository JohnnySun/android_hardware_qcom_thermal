/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Not a contribution
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Changes from Qualcomm Innovation Center are provided under the following license:

Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear */

#define LOG_TAG "thermal_hal"

#include "thermal.h"
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using ::android::OK;
using ::android::status_t;
using aidl::android::hardware::thermal::Thermal;

int main() {

	LOG(INFO) << "Thermal HAL Service AIDL starting...";

	ABinderProcess_setThreadPoolMaxThreadCount(0);
	std::shared_ptr<Thermal> therm = ndk::SharedRefBase::make<Thermal>();

	// servicemanager refuses to register a HAL-shaped name that VINTF does not
	// declare, and declaring an instance makes ThermalManagerService block on
	// it through waitForDeclaredService. A build can therefore be given a plain
	// service name instead, so the provider can be proven to start and answer
	// before anything declares it and makes the framework wait.
#ifdef ODIN_THERMAL_INSTANCE
	const std::string instance = ODIN_THERMAL_INSTANCE;
#else
	const std::string instance = std::string() + Thermal::descriptor + "/default";
#endif

	if(therm){
		binder_status_t status =
		AServiceManager_addService(therm->asBinder().get(), instance.c_str());

		if (status != STATUS_OK) {
			// Aborting here leaves init restarting a tombstone every few
			// seconds and says nothing useful.
			LOG(ERROR) << "could not register " << instance
				   << "; status=" << status;
			return EXIT_FAILURE;
		}
		therm->startInitialization();
	}

	LOG(INFO) << "Thermal HAL Service AIDL started successfully.";
	ABinderProcess_joinThreadPool();
	return EXIT_FAILURE;  // should not reach
}

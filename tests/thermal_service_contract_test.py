#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause-Clear

import re
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise AssertionError(message)


rc = (ROOT / "android.hardware.thermal-service.qti.rc").read_text()
service_match = re.search(r"^service\s+(\S+)\s+(\S+)$", rc, re.MULTILINE)
require(service_match is not None, "thermal init service declaration is missing")
service_name, service_binary = service_match.groups()
require(service_name == "vendor.thermal-hal", "AIDL thermal service name must be stable")
require(
    service_binary == "/vendor/bin/hw/android.hardware.thermal-service.qti",
    "thermal init service must execute the built AIDL HAL",
)
restart_targets = re.findall(r"^\s*restart\s+(\S+)\s*$", rc, re.MULTILINE)
require(
    restart_targets == [service_name],
    "boot action must restart the declared AIDL thermal service exactly once",
)

manifest = ElementTree.parse(ROOT / "android.hardware.thermal-service.qti.xml")
hal = manifest.getroot().find("hal")
require(hal is not None, "thermal VINTF HAL declaration is missing")
require(hal.get("format") == "aidl", "thermal VINTF HAL must use AIDL")
require(hal.findtext("name") == "android.hardware.thermal", "unexpected HAL name")
require(hal.findtext("version") == "2", "thermal AIDL version must be 2")
require(
    hal.findtext("fqname") == "IThermal/default",
    "thermal AIDL default instance is missing",
)

blueprint = (ROOT / "Android.bp").read_text()
require(
    '"qti_kernel_headers"' not in blueprint,
    "raw kernel headers must not shadow bionic UAPI headers in the thermal HAL",
)

thermal = (ROOT / "thermal.cpp").read_text()
require(
    "dummy_temp_1_0" not in thermal and "Returning Dummy Value" not in thermal,
    "the production HAL must never report a fabricated temperature",
)
require(
    thermal.count("Sensor temperature data is unavailable.") == 2,
    "both temperature APIs must fail when no real sensor value is readable",
)

for utility_name in ("thermalUtils.cpp", "thermalUtilsNetlink.cpp"):
    utility = (ROOT / utility_name).read_text()
    require(
        "cmnInst.initThreshold" not in utility,
        f"{utility_name} must not rewrite unverified kernel thermal trips",
    )
    require(
        utility.count("Skipping unreadable thermal sensor:") == 2,
        f"{utility_name} must preserve readable sensors after a peer read failure",
    )

print("thermal service contract: PASS")

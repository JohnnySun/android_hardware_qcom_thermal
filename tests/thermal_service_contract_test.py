#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause-Clear

import re
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
AIDL_INTERFACE = "android.hardware.thermal.IThermal/default"


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

odm_module = re.search(
    r'cc_binary\s*\{(?=[^}]*name:\s*"android\.hardware\.thermal-service\.qti\.odm")'
    r'[^}]*\}',
    blueprint,
    re.DOTALL,
)
require(odm_module is not None, "ODM thermal service module is missing")
odm_blueprint = odm_module.group(0)
require('stem: "android.hardware.thermal-service.qti"' in odm_blueprint,
        "ODM module must install the canonical thermal service filename")
require("device_specific: true" in odm_blueprint,
        "ODM module must install to the ODM partition")
require('"android.hardware.thermal-service.qti.odm.rc"' in odm_blueprint,
        "ODM module must use its override init contract")
require('"android.hardware.thermal-service.qti.odm.xml"' in odm_blueprint,
        "ODM module must use its HIDL-replacement VINTF contract")

odm_rc = (ROOT / "android.hardware.thermal-service.qti.odm.rc").read_text()
odm_service = re.search(r"^service\s+(\S+)\s+(\S+)$", odm_rc, re.MULTILINE)
require(odm_service is not None, "ODM thermal init service declaration is missing")
require(
    odm_service.groups()
    == ("android.thermal-hal", "/odm/bin/hw/android.hardware.thermal-service.qti"),
    "ODM thermal service must override the exact stock service from /odm",
)
require(re.search(r"^\s*override\s*$", odm_rc, re.MULTILINE) is not None,
        "ODM thermal service must use init override")
require(
    re.findall(r"^\s*interface\s+(\S+)\s+(\S+)\s*$", odm_rc, re.MULTILINE)
    == [("aidl", AIDL_INTERFACE)],
    "ODM thermal service must expose only the AIDL default interface",
)
require(re.search(r"^\s*(?:start|restart)\s+", odm_rc, re.MULTILINE) is None,
        "ODM override must not add an unproved boot start or restart action")

odm_manifest = ElementTree.parse(ROOT / "android.hardware.thermal-service.qti.odm.xml")
odm_hals = odm_manifest.getroot().findall("hal")
require(len(odm_hals) == 2, "ODM VINTF must disable HIDL before declaring AIDL")
disabled, replacement = odm_hals
require(disabled.get("format") == "hidl" and disabled.get("override") == "true",
        "first ODM thermal declaration must be the HIDL override marker")
require(disabled.findtext("name") == "android.hardware.thermal",
        "HIDL override marker must target the thermal HAL")
require(disabled.find("version") is None and disabled.find("fqname") is None,
        "HIDL override marker must clear every stock thermal version")
require(replacement.get("format") == "aidl",
        "second ODM thermal declaration must use AIDL")
require(replacement.findtext("name") == "android.hardware.thermal",
        "ODM AIDL replacement has the wrong HAL name")
require(replacement.findtext("version") == "2",
        "ODM AIDL replacement must declare version 2")
require(replacement.findtext("fqname") == "IThermal/default",
        "ODM AIDL replacement must declare IThermal/default")

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

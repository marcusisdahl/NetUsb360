#!/usr/bin/env python3
"""Checks that need no Xbox 360 SDK, run by CI on every push and pull request.

- Every source file in CUsbEnet/ is listed in CUsbEnet.vcxproj, so a new file
  can't be left out of the build.
- Every build switch in CUsbEnet/Main.h is in the README's switch table.
- Every setting Settings.cpp reads is in the README and in the sample
  release/NetUsb360.ini.
- The kernel address table has no duplicate entries.

Run it from anywhere: python3 tools/check_repo.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "CUsbEnet"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def check_project_lists_sources(errors):
    project = read(SOURCE / "CUsbEnet.vcxproj")
    listed = {m.replace("\\", "/") for m in re.findall(r'<Cl(?:Compile|Include) Include="([^"]+)"', project)}
    for path in sorted(SOURCE.rglob("*")):
        if path.suffix not in (".cpp", ".h"):
            continue
        relative = path.relative_to(SOURCE).as_posix()
        if relative not in listed:
            errors.append(f"{relative} is not listed in CUsbEnet.vcxproj")


def check_switches_documented(errors):
    readme = read(ROOT / "README.md")
    switches = re.findall(r"^#ifndef (USBENET_[A-Z0-9_]+|DEVKIT_ONLY)$", read(SOURCE / "Main.h"), re.MULTILINE)
    # Internal tuning values, not user-facing switches.
    internal = {"USBENET_RECOVER_AFTER_TIMER_GAP", "USBENET_TIMER_GAP_RECOVERY_MS"}
    for switch in switches:
        if switch not in internal and f"`{switch}`" not in readme:
            errors.append(f"build switch {switch} is missing from the README's Build switches table")


def check_settings_documented(errors):
    readme = read(ROOT / "README.md")
    sample = read(ROOT / "release" / "NetUsb360.ini")
    keys = re.findall(r'_stricmp\(Key, "([A-Za-z0-9]+)"\)', read(SOURCE / "Settings.cpp"))
    if not keys:
        errors.append("found no settings in Settings.cpp; has the parser changed?")
    for key in keys:
        if f"`{key}`" not in readme:
            errors.append(f"setting {key} is missing from the README")
        if not re.search(rf"^;?\s*{key}\s*=", sample, re.MULTILINE):
            errors.append(f"setting {key} is missing from release/NetUsb360.ini")


def check_kernel_table(errors):
    table = read(SOURCE / "KernelAddresses.cpp")
    entries = re.findall(r"\{\s*(\d+),\s*(TRUE|FALSE),", table)
    if not entries:
        errors.append("found no entries in the kernel address table")
    seen = set()
    for entry in entries:
        if entry in seen:
            errors.append(f"kernel {entry[0]} ({'devkit' if entry[1] == 'TRUE' else 'retail'}) is in the address table twice")
        seen.add(entry)


def main():
    errors = []
    check_project_lists_sources(errors)
    check_switches_documented(errors)
    check_settings_documented(errors)
    check_kernel_table(errors)
    for error in errors:
        print(f"error: {error}")
    if errors:
        return 1
    print("All repository checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

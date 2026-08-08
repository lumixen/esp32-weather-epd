#!/usr/bin/env python3
"""Broker between PlatformIO's test runner and the ESP32 QEMU emulator.

Assembles a bootable flash image (bootloader + partitions + app) from the
built test firmware and boots it in qemu-system-xtensa. The emulated serial
output is streamed to stdout where PlatformIO's Unity runner parses it.

QEMU keeps running after the tests finish (the idle emulated CPU never
exits), so the broker terminates it once the test output has settled.
"""

import argparse
import os
import re
import select
import subprocess
import sys
import time

# Hard deadline for the whole emulated run.
RUN_TIMEOUT_S = 120
# No serial output at all within this many seconds -> boot failure. The
# emulator trickles output slowly and its speed varies per run, so this only
# applies to total silence, not to missing results.
BOOT_TIMEOUT_S = 25
# Emulator is alive (boot text seen) but no test result within this many
# seconds -> the firmware is stuck.
RESULT_TIMEOUT_S = 60
# Silence after the last test result line -> tests are done. Emulated output
# can pause for several seconds between tests (String-heavy tests on the slow
# emulated CPU), so this must be generous.
DONE_SILENCE_S = 30
# Grace after Unity's end-of-run summary ("N Tests M Failures K Ignored").
SUMMARY_GRACE_S = 2
# Unity on the device prints "<file>:<line>:<name>:PASS" (and :FAIL/:IGNORED);
# PlatformIO adds [PASSED]/[FAILED]/[IGNORED] markers on top.
RESULT_RE = re.compile(r"(\[PASSED\]|\[FAILED\]|\[IGNORED\]|:PASS\b|:FAIL\b|:IGNORED\b)")
# Unity's summary line contains all three words, e.g. "12 Tests 0 Failures 0 Ignored".
SUMMARY_WORDS = ("Tests", "Failures", "Ignored")


def build_flash_image(args):
    flash = os.path.join(args.build_dir, "qemu_flash.bin")
    merge = [
        sys.executable,
        args.esptool,
        "--chip",
        "esp32",
        "merge_bin",
        "--output",
        flash,
        "--fill-flash-size",
        "4MB",
    ]
    entries = [
        ("0x1000", "bootloader.bin"),
        ("0x8000", "partitions.bin"),
        ("0xe000", "boot_app0.bin"),
        ("0x10000", "firmware.bin"),
    ]
    for offset, name in entries:
        path = os.path.join(args.build_dir, name)
        if not os.path.exists(path):
            path = fallback_image_path(args, name)
            if not path:
                raise SystemExit("missing build image: %s" % name)
        merge += [str(int(offset, 0)), path]
    subprocess.check_call(merge, stdout=subprocess.DEVNULL)
    return flash


def fallback_image_path(args, name):
    """Locate framework-provided images (boot_app0.bin) not copied by pio."""
    for candidate in (
        os.path.join(args.packages_dir, "framework-arduinoespressif32", "tools", "partitions", name),
    ):
        if os.path.exists(candidate):
            return candidate
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True, help="path to qemu-system-xtensa")
    parser.add_argument("--esptool", required=True, help="path to esptool.py")
    parser.add_argument(
        "--build-dir", required=True, help="env build directory (.pio/build/<env>)"
    )
    parser.add_argument(
        "--packages-dir",
        default=os.path.join(os.path.expanduser("~"), ".platformio", "packages"),
        help="PlatformIO packages directory (default: ~/.platformio/packages)",
    )
    args = parser.parse_args()

    flash = build_flash_image(args)

    cmd = [
        args.qemu,
        "-nographic",
        "-machine",
        "esp32",
        "-drive",
        "file=%s,if=mtd,format=raw" % flash,
        "-global",
        "driver=timer.esp32.timg,property=wdt_disable,value=true",
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)

    start = time.time()
    any_output = False
    result_since = start
    seen_result = False
    quiet_since = start
    summary_at = None
    tail = ""
    exit_code = 0

    while True:
        elapsed = time.time() - start
        if elapsed > RUN_TIMEOUT_S:
            print("ERROR: emulator run timed out after %ds" % RUN_TIMEOUT_S, flush=True)
            exit_code = 1
            break
        if not any_output and elapsed > BOOT_TIMEOUT_S:
            print("ERROR: no serial output within %ds" % BOOT_TIMEOUT_S, flush=True)
            exit_code = 1
            break
        if any_output and not seen_result and time.time() - result_since > RESULT_TIMEOUT_S:
            print("ERROR: no test result within %ds" % RESULT_TIMEOUT_S, flush=True)
            exit_code = 1
            break
        if seen_result and time.time() - quiet_since > DONE_SILENCE_S:
            break
        if summary_at is not None and time.time() - summary_at > SUMMARY_GRACE_S:
            break

        ready, _, _ = select.select([proc.stdout], [], [], 0.5)
        if not ready:
            continue

        chunk = os.read(proc.stdout.fileno(), 4096)
        if not chunk:
            break  # the emulator exited on its own
        any_output = True
        sys.stdout.buffer.write(chunk)
        sys.stdout.buffer.flush()
        text = chunk.decode("utf-8", errors="replace")
        tail = (tail + text)[-256:]

        if RESULT_RE.search(text):
            if not seen_result:
                seen_result = True
            quiet_since = time.time()
        if all(word in tail for word in SUMMARY_WORDS) and summary_at is None:
            summary_at = time.time()

    proc.kill()
    proc.wait()
    return exit_code


if __name__ == "__main__":
    sys.exit(main())

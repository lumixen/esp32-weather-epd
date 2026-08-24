#!/usr/bin/env python3
"""Broker between PlatformIO's test runner and the ESP32 QEMU emulator.

Assembles a bootable flash image (bootloader + partitions + app) from the
built test firmware and boots it in qemu-system-xtensa. The emulated serial
output is streamed to stdout where PlatformIO's Unity runner parses it.

QEMU keeps running after the tests finish (the idle emulated CPU never
exits), so the broker terminates it once the test output has settled.
"""

import argparse
import atexit
import os
import re
import select
import signal
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
# Once a panic marker is seen, keep reading the merged QEMU stream. The panic
# handler emits the register dump and backtrace after the marker, and killing
# QEMU immediately loses that output. The quiet limit handles a rebooting
# firmware, while the hard limit prevents a broken emulator from hanging CI.
CRASH_QUIET_S = 15
CRASH_CAPTURE_TIMEOUT_S = 30
# Unity's summary is emitted only after every test result has been flushed.
# Terminate immediately when it is observed; waiting here lets the ESP32 test
# image enter its post-Unity idle/reboot path and duplicate result lines.
SUMMARY_GRACE_S = 0
# Unity on the device prints "<file>:<line>:<name>:PASS" (and :FAIL/:IGNORED);
# PlatformIO adds [PASSED]/[FAILED]/[IGNORED] markers on top.
RESULT_RE = re.compile(r"(\[PASSED\]|\[FAILED\]|\[IGNORED\]|:PASS\b|:FAIL\b|:IGNORED\b)")
# Panic text is emitted by the ESP32 runtime on the emulated serial stream.
# Detect it here so PlatformIO reports the actual crash instead of only a
# later timeout/SIGHUP after QEMU restarts the firmware.
CRASH_MARKERS = (
    b"Guru Meditation Error",
    b"abort() was called",
    b"assert failed",
    b"Stack smashing protect failure",
    b"CORRUPT HEAP",
    b"Assertion `",
    b"assertion failed",
    b"Core  0 register dump:",
    b"Core 0 register dump:",
    b"Backtrace:",
    b"Program received signal",
    b"failed to calculate modulo inverse",
)
# Unity's summary line has this form, e.g. "12 Tests 0 Failures 0 Ignored".
SUMMARY_RE = re.compile(rb"\d+ Tests \d+ Failures \d+ Ignored")


def build_flash_image(args):
    flash = os.path.join(args.build_dir, "qemu_flash.bin")
    merge = [args.esptool]
    # Plain .py scripts need an interpreter; console-script binaries (e.g.
    # the PlatformIO venv's esptool, which has the deps of esptool v5) run
    # directly.
    if args.esptool.endswith(".py"):
        merge = [sys.executable] + merge
    merge += [
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
    parser.add_argument(
        "--log-file",
        help="file receiving the complete merged QEMU output (or QEMU_LOG_FILE)",
    )
    parser.add_argument(
        "--debug-file",
        help="QEMU guest-error diagnostics file (or QEMU_DEBUG_FILE)",
    )
    args = parser.parse_args()

    flash = build_flash_image(args)
    log_file = args.log_file or os.environ.get("QEMU_LOG_FILE")
    if not log_file:
        log_file = os.path.join(args.build_dir, "qemu_output.log")
    debug_file = args.debug_file or os.environ.get("QEMU_DEBUG_FILE")
    if not debug_file:
        debug_file = os.path.join(args.build_dir, "qemu_debug.log")
    log_dir = os.path.dirname(os.path.abspath(log_file))
    debug_dir = os.path.dirname(os.path.abspath(debug_file))
    os.makedirs(log_dir, exist_ok=True)
    os.makedirs(debug_dir, exist_ok=True)

    cmd = [
        args.qemu,
        "-nographic",
        "-machine",
        "esp32",
        "-drive",
        "file=%s,if=mtd,format=raw" % flash,
        "-global",
        "driver=timer.esp32.timg,property=wdt_disable,value=true",
        "-d",
        "guest_errors",
        "-D",
        debug_file,
    ]
    # Merge stderr into the serial stream. Keeping it in a separate pipe meant
    # that normal QEMU diagnostics were silently discarded, and the broker
    # could kill QEMU while unread stderr still contained the backtrace.
    output_log = open(log_file, "wb")
    print("QEMU output log: %s" % log_file, flush=True)
    print("QEMU debug log: %s" % debug_file, flush=True)

    # Use a process group so a stuck QEMU (or a child it created) cannot keep
    # the PlatformIO test command alive after a crash/timeout. Open the log
    # before spawning QEMU so a log-file setup failure cannot orphan it.
    process_holder = [None]

    def cleanup_process():
        terminated = True
        try:
            proc = process_holder[0]
            if proc is not None and proc.poll() is None:
                # Ask QEMU to exit cleanly after the stream has been drained.
                # Fall back to SIGKILL if it does not exit promptly.
                try:
                    os.killpg(proc.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(proc.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    try:
                        proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        print("ERROR: failed to terminate emulator after test failure", flush=True)
                        terminated = False
        finally:
            # Keep this in finally: preserving the diagnostic log is more
            # important when the final termination attempt itself times out.
            output_log.close()
        return terminated

    # Protect against exceptions while reading/writing the diagnostic stream
    # (for example, a full disk). The broker must not leave QEMU orphaned.
    atexit.register(cleanup_process)
    try:
        process_holder[0] = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True
        )
    except BaseException:
        cleanup_process()
        atexit.unregister(cleanup_process)
        raise
    proc = process_holder[0]

    start = time.time()
    any_output = False
    result_since = start
    seen_result = False
    quiet_since = start
    summary_at = None
    tail = b""
    crash_detected_at = None
    last_output_at = start
    exit_code = 0

    while True:
        now = time.time()
        elapsed = now - start
        if crash_detected_at is not None:
            # A panic is printed asynchronously. Drain the stream until it is
            # quiet, but never wait indefinitely for a rebooting emulator.
            if now - last_output_at >= CRASH_QUIET_S or now - crash_detected_at >= CRASH_CAPTURE_TIMEOUT_S:
                break
        elif elapsed > RUN_TIMEOUT_S:
            print("ERROR: emulator run timed out after %ds" % RUN_TIMEOUT_S, flush=True)
            exit_code = 1
            break
        elif not any_output and elapsed > BOOT_TIMEOUT_S:
            print("ERROR: no serial output within %ds" % BOOT_TIMEOUT_S, flush=True)
            exit_code = 1
            break
        elif any_output and not seen_result and now - result_since > RESULT_TIMEOUT_S:
            print("ERROR: no test result within %ds" % RESULT_TIMEOUT_S, flush=True)
            if tail:
                print("Last serial output before timeout:", flush=True)
                print(tail.decode("utf-8", errors="replace"), end="" if tail.endswith(b"\n") else "\n", flush=True)
            print("Complete emulator output saved to %s" % log_file, flush=True)
            print("QEMU debug output saved to %s" % debug_file, flush=True)
            print("QEMU process status: %s" % (proc.poll() if proc.poll() is not None else "still running"), flush=True)
            exit_code = 1
            break
        elif seen_result and now - quiet_since > DONE_SILENCE_S:
            print("ERROR: emulator went silent before Unity summary", flush=True)
            if tail:
                print("Last serial output:", flush=True)
                print(tail.decode("utf-8", errors="replace"), end="" if tail.endswith(b"\n") else "\n", flush=True)
            print("Complete emulator output saved to %s" % log_file, flush=True)
            print("QEMU debug output saved to %s" % debug_file, flush=True)
            print("QEMU process status: %s" % (proc.poll() if proc.poll() is not None else "still running"), flush=True)
            exit_code = 1
            break
        elif summary_at is not None and now - summary_at >= SUMMARY_GRACE_S:
            break

        ready, _, _ = select.select([proc.stdout], [], [], 0.5)
        if not ready:
            continue

        chunk = os.read(proc.stdout.fileno(), 4096)
        if not chunk:
            # An emulator exit before Unity's summary is a test failure. The
            # merged stream has already been drained, so the log contains all
            # diagnostics that QEMU managed to emit.
            return_code = proc.poll()
            if return_code is None:
                try:
                    return_code = proc.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    return_code = "unknown"
            if crash_detected_at is None and summary_at is None:
                print("ERROR: emulator exited before Unity summary (status %s)" % return_code, flush=True)
                print("Complete emulator output saved to %s" % log_file, flush=True)
                print("QEMU debug output saved to %s" % debug_file, flush=True)
                exit_code = 1
            break

        # Save every byte before inspecting it. In particular, do not truncate
        # the log at the first panic marker: the register dump and backtrace
        # follow that marker and are often split across reads. Keep the old
        # summary truncation only for PlatformIO's parser, so post-summary
        # reboot noise cannot create duplicate Unity results.
        raw_chunk = chunk
        combined = tail + raw_chunk
        summary_match = SUMMARY_RE.search(combined)
        summary_complete = False
        forward_chunk = raw_chunk
        if summary_match is not None:
            line_end = combined.find(b"\n", summary_match.end())
            if line_end >= 0:
                summary_complete = True
                # Only emit the new portion of the combined buffer. The tail
                # was already forwarded during the previous iteration.
                chunk_end = line_end + 1
                forward_chunk = combined[len(tail) : chunk_end] if chunk_end > len(tail) else b""

        output_log.write(raw_chunk)
        output_log.flush()
        sys.stdout.buffer.write(forward_chunk)
        sys.stdout.buffer.flush()
        any_output = True
        last_output_at = time.time()

        text = forward_chunk.decode("utf-8", errors="replace")
        crash_marker = next((marker for marker in CRASH_MARKERS if marker in combined), None)
        if crash_marker is not None and crash_detected_at is None:
            crash_detected_at = time.time()
            exit_code = 1

        tail = (tail + forward_chunk)[-4096:]

        if RESULT_RE.search(text):
            if not seen_result:
                seen_result = True
            quiet_since = time.time()
        if summary_complete and summary_at is None:
            summary_at = time.time()
        if summary_complete and crash_detected_at is None:
            break

    if crash_detected_at is not None:
        print("ERROR: emulator reported firmware crash; complete output saved to %s" % log_file, flush=True)
        print("QEMU debug output saved to %s" % debug_file, flush=True)

    if not cleanup_process():
        exit_code = 1
    atexit.unregister(cleanup_process)
    return exit_code


if __name__ == "__main__":
    sys.exit(main())

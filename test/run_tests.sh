#!/usr/bin/env bash
#
# Runs the ESP32 QEMU unit tests for every pinned test config. The config is
# baked into the firmware at compile time, so each config reuses the single
# lolin_d32_qemu env with a different configuration. Each configuration has
# one PlatformIO suite containing feature-level .inc modules; this keeps one
# build and one QEMU boot per config while Unity still reports every test:
#
#   1. openmeteo       - test/configs/openmeteo.yml (the env default, wired in
#      by scripts/select_test_env_config.py when ESP32_EPD_CONFIG is unset).
#   2. owm             - test/configs/owm.yml via ESP32_EPD_CONFIG.
#   3. noaa            - test/configs/noaa.yml via ESP32_EPD_CONFIG.
#
# The root suite selected for each run contains only test modules compatible
# with that configuration. All runs always execute (a failing run does not
# skip the next config); the script exits non-zero if any of them failed.
#
# Works inside the Docker test container (test/run_tests_docker.sh) and on a
# host with PlatformIO + QEMU available (test_testing_command in
# platformio.ini points at /opt/qemu - adjust for a host setup).
#
# Environment:
#   PIO_BIN   PlatformIO executable (default: pio on PATH)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PIO="${PIO_BIN:-pio}"
EXTRA_ARGS=("$@")

cd "$ROOT"

# Avoid uploading diagnostics left by a previous cached build if a test build
# fails before the broker gets a chance to truncate its log.
QEMU_BUILD_DIR=".pio/build/lolin_d32_qemu"
FIRMWARE_ELF="$QEMU_BUILD_DIR/firmware.elf"
rm -f "$QEMU_BUILD_DIR"/qemu_output*.log \
    "$QEMU_BUILD_DIR"/qemu_debug*.log \
    "$QEMU_BUILD_DIR"/firmware_openmeteo.elf \
    "$QEMU_BUILD_DIR"/firmware_owm.elf \
    "$QEMU_BUILD_DIR"/firmware_noaa.elf

echo "=================================================="
echo "  test run: config openmeteo (test/configs/openmeteo.yml)"
echo "=================================================="
status=0

# Remove the shared ELF before each build. This prevents a failed build from
# being mistaken for the firmware produced for the current configuration.
rm -f "$FIRMWARE_ELF"
QEMU_LOG_FILE="$QEMU_BUILD_DIR/qemu_output_openmeteo.log" \
QEMU_DEBUG_FILE="$QEMU_BUILD_DIR/qemu_debug_openmeteo.log" \
    "$PIO" test -e lolin_d32_qemu --without-uploading \
    -f test_openmeteo \
    "${EXTRA_ARGS[@]}" || status=1
if [[ -f "$FIRMWARE_ELF" ]]; then
    cp "$FIRMWARE_ELF" "$QEMU_BUILD_DIR/firmware_openmeteo.elf"
else
    echo "WARNING: no Open-Meteo firmware ELF was produced" >&2
fi

echo "=================================================="
echo "  test run: config owm (test/configs/owm.yml)"
echo "=================================================="
rm -f "$FIRMWARE_ELF"
ESP32_EPD_CONFIG=test/configs/owm.yml \
QEMU_LOG_FILE="$QEMU_BUILD_DIR/qemu_output_owm.log" \
QEMU_DEBUG_FILE="$QEMU_BUILD_DIR/qemu_debug_owm.log" \
    "$PIO" test -e lolin_d32_qemu --without-uploading \
    -f test_owm \
    "${EXTRA_ARGS[@]}" || status=1
if [[ -f "$FIRMWARE_ELF" ]]; then
    cp "$FIRMWARE_ELF" "$QEMU_BUILD_DIR/firmware_owm.elf"
else
    echo "WARNING: no OWM firmware ELF was produced" >&2
fi

echo "=================================================="
echo "  test run: config noaa (test/configs/noaa.yml)"
echo "=================================================="
rm -f "$FIRMWARE_ELF"
ESP32_EPD_CONFIG=test/configs/noaa.yml \
QEMU_LOG_FILE="$QEMU_BUILD_DIR/qemu_output_noaa.log" \
QEMU_DEBUG_FILE="$QEMU_BUILD_DIR/qemu_debug_noaa.log" \
    "$PIO" test -e lolin_d32_qemu --without-uploading \
    -f test_noaa \
    "${EXTRA_ARGS[@]}" || status=1
if [[ -f "$FIRMWARE_ELF" ]]; then
    cp "$FIRMWARE_ELF" "$QEMU_BUILD_DIR/firmware_noaa.elf"
else
    echo "WARNING: no NOAA firmware ELF was produced" >&2
fi

exit "$status"

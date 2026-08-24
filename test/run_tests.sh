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

echo "=================================================="
echo "  test run: config openmeteo (test/configs/openmeteo.yml)"
echo "=================================================="
status=0
"$PIO" test -e lolin_d32_qemu --without-uploading \
    -f test_openmeteo \
    "${EXTRA_ARGS[@]}" || status=1

echo "=================================================="
echo "  test run: config owm (test/configs/owm.yml)"
echo "=================================================="
ESP32_EPD_CONFIG=test/configs/owm.yml \
    "$PIO" test -e lolin_d32_qemu --without-uploading \
    -f test_owm \
    "${EXTRA_ARGS[@]}" || status=1

exit "$status"

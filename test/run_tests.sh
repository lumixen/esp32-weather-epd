#!/usr/bin/env bash
#
# Runs the ESP32 QEMU unit tests for every test config. The config is baked
# into the firmware at compile time, so each config reuses the single
# lolin_d32_qemu env and recompiles it with a different configuration. Each
# run lists its suites explicitly with the CLI filter (-f), so every suite
# runs exactly once and only under a config that can link it:
#
#   1. openmeteo - test/configs/openmeteo.yml (the env default, wired in by
#      scripts/select_test_env_config.py when ESP32_EPD_CONFIG is unset).
#      Runs the config-independent suites plus the Open-Meteo provider suite
#      (skipping the OWM provider, whose sources do not compile here).
#   2. owm       - test/configs/owm.yml via ESP32_EPD_CONFIG. Runs only the
#      OWM provider suite, which needs the OWM provider sources.
#
# Adding a suite = one -f to the runs it must execute under. Both runs always
# execute (a failing run does not skip the next config); the script exits
# non-zero if any of them failed.
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
    -f test_display_utils \
    -f test_rtc_drift_correction \
    -f test_moon_tools \
    -f test_open_meteo_weather_provider \
    -f test_open_meteo_air_quality_provider \
    -f test_meteoalarm \
    "${EXTRA_ARGS[@]}" || status=1

echo "=================================================="
echo "  test run: config owm (test/configs/owm.yml)"
echo "=================================================="
ESP32_EPD_CONFIG=test/configs/owm.yml \
    "$PIO" test -e lolin_d32_qemu --without-uploading \
    -f test_owm_weather_provider \
    "${EXTRA_ARGS[@]}" || status=1

exit "$status"
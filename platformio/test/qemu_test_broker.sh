#!/usr/bin/env bash
# Launcher for qemu_test_broker.py, invoked by PlatformIO as a single
# executable: `test_testing_command` cannot carry arguments (pio 6.1 executes
# it without shell splitting). All paths are computed for the docker-only
# setup: QEMU is baked into the image at /opt/qemu, PlatformIO packages live
# in the container HOME, and the build dir sits inside the mounted project.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

exec python3 "$ROOT/test/qemu_test_broker.py" \
    --qemu /opt/qemu/bin/qemu-system-xtensa \
    --esptool "$HOME/.platformio/packages/tool-esptoolpy/esptool.py" \
    --build-dir "$ROOT/.pio/build/lolin_d32_qemu"

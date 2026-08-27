#!/usr/bin/env bash
#
# Runs the QEMU unit tests inside Docker with the ESP32 QEMU emulator.
# The whole project directory is mounted read-write, so build artifacts land
# in .pio/ on the host and the built image is cached across runs.
#
# The actual test orchestration lives in test/run_tests.sh (executed in the
# container): it reads the registry in test/configs/index.yml and runs all or
# the selected configuration-rooted PlatformIO suites. Each run selects one
# compatible suite, so its feature modules share one build and one QEMU boot.
# Use --config <id> to focus on one configuration; repeat it for a subset.
set -euo pipefail

# Root of the git repo: the container mounts it read-write at /project, so
# build artifacts land in .pio/ on the host.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Listing and help do not need Docker or PlatformIO. Keep these fast paths in
# the wrapper so developers can inspect the registry before pulling/building
# the test image.
if [[ $# -eq 1 && ( "$1" == "--list" || "$1" == "--help" || "$1" == "-h" ) ]]; then
    exec bash "$ROOT/test/run_tests.sh" "$1"
fi

IMAGE="${IMAGE:-esp32-weather-epd-test}"

case "$(uname -m)" in
    arm64|aarch64) PLATFORM="linux/arm64" ;;
    *) PLATFORM="linux/amd64" ;;
esac

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "Building test image $IMAGE ..."
    docker build --platform "$PLATFORM" -t "$IMAGE" "$ROOT/test/docker"
fi

# Everything runs inside the container: PlatformIO's own packages dir is
# persisted in a named volume (so toolchains are downloaded only once), while
# QEMU itself is baked into the image at /opt/qemu.
# PIO_HOST_DIR overrides the named volume with a host directory (used in CI
# where the volume cannot persist between runs); local usage is unchanged.
if [ -n "${PIO_HOST_DIR:-}" ]; then
    mkdir -p "$PIO_HOST_DIR"
    PIO_VOLUME_ARGS=(-v "$PIO_HOST_DIR:/root/.platformio")
else
    PIO_VOLUME_ARGS=(-v "esp32-weather-epd-pio:/root/.platformio")
fi

exec docker run --rm --platform "$PLATFORM" \
    -v "$ROOT:/project" \
    "${PIO_VOLUME_ARGS[@]}" \
    -w /project \
    "$IMAGE" \
    bash test/run_tests.sh "$@"

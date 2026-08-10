#!/usr/bin/env bash
#
# Installs the Espressif ESP32 QEMU binary into PlatformIO's packages dir.
#
# Usage: install_qemu.sh [version]
#   version  release tag on github.com/espressif/qemu (default below)
set -euo pipefail

VERSION="${1:-esp-develop-9.2.2-20260417}"
DEST="${DEST:-$HOME/.platformio/packages/tool-qemu-xtensa}"

case "$(uname -s)-$(uname -m)" in
    Linux-x86_64|Linux-amd64) TARGET="x86_64-linux-gnu" ;;
    Linux-aarch64|Linux-arm64) TARGET="aarch64-linux-gnu" ;;
    Darwin-x86_64) TARGET="x86_64-apple-darwin" ;;
    Darwin-arm64|Darwin-aarch64) TARGET="aarch64-apple-darwin" ;;
    *)
        echo "Unsupported platform: $(uname -s) $(uname -m)" >&2
        exit 1
        ;;
esac

if [ -x "$DEST/bin/qemu-system-xtensa" ]; then
    echo "QEMU already installed at $DEST"
    exit 0
fi

PREFIX="$(printf '%s' "$VERSION" | tr '-' '_')"
URL="https://github.com/espressif/qemu/releases/download/${VERSION}/qemu-xtensa-softmmu-${PREFIX}-${TARGET}.tar.xz"

echo "Downloading $URL"
mkdir -p "$(dirname "$DEST")"
curl --fail --location --silent --show-error --output /tmp/qemu.tar.xz "$URL"
rm -rf /tmp/qemu-extract "$DEST"
mkdir -p /tmp/qemu-extract
tar -xJf /tmp/qemu.tar.xz -C /tmp/qemu-extract
mv /tmp/qemu-extract/qemu "$DEST"
rm -rf /tmp/qemu-extract /tmp/qemu.tar.xz

if [ ! -x "$DEST/bin/qemu-system-xtensa" ]; then
    echo "ERROR: qemu-system-xtensa not found in $DEST" >&2
    exit 1
fi

"$DEST/bin/qemu-system-xtensa" --version | head -n 1
echo "QEMU installed at $DEST"

#!/usr/bin/env bash
#
# Runs the ESP32 QEMU unit tests for the registered test configurations. The
# config is baked into the firmware at compile time, so each selected config
# reuses the single lolin_d32_qemu env with a different configuration. Each
# configuration has one PlatformIO suite containing feature-level .inc
# modules; this keeps one build and one QEMU boot per config while Unity still
# reports every test.
#
# The registry in test/configs/index.yml is the source of truth for config
# paths, suite names, ordering, and the default config. A failing run does not
# skip the next selected config; the script exits non-zero if any run fails.
#
# Works inside the Docker test container (test/run_tests_docker.sh) and on a
# host with PlatformIO + QEMU available (test_testing_command in
# platformio.ini points at /opt/qemu - adjust for a host setup).
#
# Runner options:
#   --config ID   Run one configuration; may be repeated.
#   --all         Run every registered configuration (the default).
#   --list        List registered configuration IDs and exit.
#   --help        Show this help and exit.
#
# Environment:
#   PIO_BIN       PlatformIO executable (default: pio on PATH)
#   PYTHON_BIN    Python executable for the registry helper (default: python3)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PIO="${PIO_BIN:-pio}"
if [[ -n "${PYTHON_BIN:-}" ]]; then
    PYTHON="$PYTHON_BIN"
elif [[ -x "$HOME/.platformio/penv/bin/python" ]]; then
    PYTHON="$HOME/.platformio/penv/bin/python"
else
    PYTHON="python3"
fi

usage() {
    sed -n '2,30p' "$0"
    cat <<'EOF'

Examples:
  bash test/run_tests_docker.sh
  bash test/run_tests_docker.sh --config owm3 -v
  bash test/run_tests_docker.sh --config openmeteo --config noaa
  bash test/run_tests_docker.sh --all
EOF
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi

if [[ "${1:-}" == "--list" && $# -eq 1 ]]; then
    exec "$PYTHON" "$ROOT/scripts/test_configs.py" --list
fi

requested_ids=()
all_requested=0
extra_args=()
while (($#)); do
    case "$1" in
        --config)
            if (($# < 2)); then
                echo "ERROR: --config requires an ID" >&2
                usage >&2
                exit 2
            fi
            requested_ids+=("$2")
            shift 2
            ;;
        --config=*)
            requested_ids+=("${1#*=}")
            shift
            ;;
        --all)
            if ((${#requested_ids[@]})); then
                echo "ERROR: --all cannot be combined with --config" >&2
                exit 2
            fi
            all_requested=1
            shift
            ;;
        --list)
            if (($# != 1)); then
                echo "ERROR: --list cannot be combined with test options" >&2
                exit 2
            fi
            exec "$PYTHON" "$ROOT/scripts/test_configs.py" --list
            ;;
        --)
            shift
            extra_args+=("$@")
            break
            ;;
        *)
            extra_args+=("$1")
            shift
            ;;
    esac
done

if ((all_requested && ${#requested_ids[@]})); then
    echo "ERROR: --all cannot be combined with --config" >&2
    exit 2
fi

cd "$ROOT"

registry_data=$("$PYTHON" "$ROOT/scripts/test_configs.py" --shell)
declare -a all_ids=()
declare -a config_files=()
declare -a suites=()
while IFS=$'\t' read -r config_id config_file suite; do
    [[ -z "$config_id" ]] && continue
    all_ids+=("$config_id")
    config_files+=("$config_file")
    suites+=("$suite")
done <<< "$registry_data"

selected_ids=()
if ((${#requested_ids[@]} == 0 || all_requested)); then
    selected_ids=("${all_ids[@]}")
else
    selected_ids=()
    for config_id in "${requested_ids[@]}"; do
        config_index=-1
        for i in "${!all_ids[@]}"; do
            if [[ "${all_ids[$i]}" == "$config_id" ]]; then
                config_index=$i
                break
            fi
        done
        if ((config_index < 0)); then
            echo "ERROR: unknown test configuration '$config_id'" >&2
            echo "Available configurations: ${all_ids[*]}" >&2
            exit 2
        fi
        if ((${#selected_ids[@]})); then
            for selected_id in "${selected_ids[@]}"; do
                if [[ "$selected_id" == "$config_id" ]]; then
                    echo "ERROR: test configuration '$config_id' was requested more than once" >&2
                    exit 2
                fi
            done
        fi
        selected_ids+=("$config_id")
    done
fi

QEMU_BUILD_DIR=".pio/build/lolin_d32_qemu"
FIRMWARE_ELF="$QEMU_BUILD_DIR/firmware.elf"
status=0

for config_id in "${selected_ids[@]}"; do
    config_index=-1
    for i in "${!all_ids[@]}"; do
        if [[ "${all_ids[$i]}" == "$config_id" ]]; then
            config_index=$i
            break
        fi
    done
    config_file="${config_files[$config_index]}"
    suite="${suites[$config_index]}"
    qemu_log="$QEMU_BUILD_DIR/qemu_output_${config_id}.log"
    qemu_debug="$QEMU_BUILD_DIR/qemu_debug_${config_id}.log"
    firmware_copy="$QEMU_BUILD_DIR/firmware_${config_id}.elf"

    echo "=================================================="
    echo "  test run: config $config_id ($config_file)"
    echo "  test suite: $suite"
    echo "=================================================="

    # Avoid uploading diagnostics left by a previous cached build if a test
    # build fails before the broker gets a chance to truncate its log.
    rm -f "$qemu_log" "$qemu_debug" "$firmware_copy"

    # Remove the shared ELF before each build. This prevents a failed build
    # from being mistaken for the firmware produced for the current config.
    rm -f "$FIRMWARE_ELF"
    pio_command=("$PIO" test -e lolin_d32_qemu --without-uploading -f "$suite")
    if ((${#extra_args[@]})); then
        pio_command+=("${extra_args[@]}")
    fi
    if ESP32_EPD_CONFIG="$config_file" \
        QEMU_LOG_FILE="$qemu_log" \
        QEMU_DEBUG_FILE="$qemu_debug" \
        "${pio_command[@]}"; then
        :
    else
        status=1
    fi

    if [[ -f "$FIRMWARE_ELF" ]]; then
        cp "$FIRMWARE_ELF" "$firmware_copy"
    else
        echo "WARNING: no $config_id firmware ELF was produced" >&2
    fi
done

exit "$status"

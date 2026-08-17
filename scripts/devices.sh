#!/usr/bin/env bash
#
# PlatformIO wrapper with per-device config management.
#
# Devices are configured in devices/<name>.yml (gitignored, one fully
# independent config per device). This script resolves a device's config,
# sets ESP32_EPD_CONFIG, and invokes PlatformIO.
#
# Usage:
#   devices.sh list                            list known devices
#   devices.sh list-envs                       list envs defined in platformio.ini
#   devices.sh validate <name|path>            validate a device config
#   devices.sh build <name|path> [--env <env>] [pio args...]
#   devices.sh flash <name|path> [--env <env>] [port]
#   devices.sh monitor <name|path> [--env <env>] [port]
#   devices.sh flash-monitor <name|path> [--env <env>] [port]
#   devices.sh run <pio args...>               pass-through to pio
#
# Environment:
#   PIO_BIN          PlatformIO executable (default: pio on PATH, else
#                    ~/.platformio/penv/bin/pio)
#   ESP32_EPD_CONFIG Device name or config path (default: command argument)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DEFAULT_ENV="lolin_d32"

usage() {
    cat >&2 <<'EOF'
Usage:
  devices.sh list
  devices.sh list-envs
  devices.sh validate <name|path>
  devices.sh build <name|path> [--env <env>] [pio args...]
  devices.sh flash <name|path> [--env <env>] [port]
  devices.sh monitor <name|path> [--env <env>] [port]
  devices.sh flash-monitor <name|path> [--env <env>] [port]
  devices.sh run <pio args...>
EOF
    exit 1
}

find_pio() {
    if [[ -n "${PIO_BIN:-}" && -x "$PIO_BIN" ]]; then
        printf '%s\n' "$PIO_BIN"
        return
    fi
    local pio
    if pio="$(command -v pio 2>/dev/null)" && [[ -n "$pio" ]]; then
        printf '%s\n' "$pio"
        return
    fi
    if [[ -x "$HOME/.platformio/penv/bin/pio" ]]; then
        printf '%s\n' "$HOME/.platformio/penv/bin/pio"
        return
    fi
    echo "error: PlatformIO not found; set PIO_BIN or install it" >&2
    exit 1
}

find_pio_python() {
    local py
    py="$(dirname "$PIO")/python"
    if [[ -x "$py" ]]; then
        printf '%s\n' "$py"
        return
    fi
    py="$(command -v python3 2>/dev/null || true)"
    if [[ -n "$py" ]]; then
        printf '%s\n' "$py"
        return
    fi
    echo "error: no python interpreter found for config validation" >&2
    exit 1
}

resolve_config() {
    local spec="${1:-}"
    if [[ -z "$spec" ]]; then
        echo "error: missing device name or config path" >&2
        usage
    fi
    if [[ "$spec" == */* ]]; then
        printf '%s\n' "$spec"
    else
        printf '%s\n' "$PROJECT_DIR/devices/${spec}.yml"
    fi
}

# Extract --env [value] from arguments into $ENV_SELECTION, leaving the
# remaining positional arguments in POS_ARGS.
parse_env() {
    ENV_SELECTION="$DEFAULT_ENV"
    POS_ARGS=()
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --env)
                [[ $# -ge 2 ]] || usage
                ENV_SELECTION="$2"
                shift 2
                ;;
            --env=*)
                ENV_SELECTION="${1#*=}"
                shift
                ;;
            *)
                POS_ARGS+=("$1")
                shift
                ;;
        esac
    done
}

run_pio() {
    local config="${1:?missing config}"
    local env="$2"
    shift 2
    local config_path
    config_path="$(resolve_config "$config")"
    if [[ ! -f "$config_path" ]]; then
        echo "error: config file not found: $config_path" >&2
        exit 1
    fi
    ESP32_EPD_CONFIG="$config_path" "$PIO" run -e "$env" "$@"
}

PIO="$(find_pio)"
PIO_PYTHON="$(find_pio_python)"

cmd="${1:-}"
case "$cmd" in
    list)
        echo "Known devices (devices/*.yml):"
        found=0
        for f in "$PROJECT_DIR"/devices/*.yml; do
            [[ -e "$f" ]] || continue
            case "$(basename "$f")" in
                *.example.yml) continue ;;
            esac
            printf '  %s\n' "$(basename "$f" .yml)"
            found=1
        done
        [[ $found -eq 1 ]] || echo "  (none)"
        ;;
    list-envs)
        echo "Envs defined in platformio.ini:"
        grep -o '^\[env:[^]]*\]' "$PROJECT_DIR/platformio.ini" 2>/dev/null \
            | sed 's/\[env://; s/\]//' \
            | while read -r e; do printf '  %s\n' "$e"; done || true
        ;;
    validate)
        [[ $# -ge 2 ]] || usage
        config_path="$(resolve_config "$2")"
        if [[ ! -f "$config_path" ]]; then
            echo "error: config file not found: $config_path" >&2
            exit 1
        fi
        "$PIO_PYTHON" "$SCRIPT_DIR/config.py" --validate "$config_path"
        ;;
    build)
        parse_env "${@:2}"
        [[ ${#POS_ARGS[@]} -ge 1 ]] || usage
        run_pio "${POS_ARGS[0]}" "$ENV_SELECTION" "${POS_ARGS[@]:1}"
        ;;
    flash)
        parse_env "${@:2}"
        [[ ${#POS_ARGS[@]} -ge 1 ]] || usage
        port="${POS_ARGS[1]:-}"
        run_pio "${POS_ARGS[0]}" "$ENV_SELECTION" -t upload ${port:+--upload-port "$port"}
        ;;
    monitor)
        parse_env "${@:2}"
        [[ ${#POS_ARGS[@]} -ge 1 ]] || usage
        config_path="$(resolve_config "${POS_ARGS[0]}")"
        port="${POS_ARGS[1]:-}"
        if [[ ! -f "$config_path" ]]; then
            echo "error: config file not found: $config_path" >&2
            exit 1
        fi
        ESP32_EPD_CONFIG="$config_path" "$PIO" device monitor ${port:+-p "$port"} "${POS_ARGS[@]:2}"
        ;;
    flash-monitor)
        parse_env "${@:2}"
        [[ ${#POS_ARGS[@]} -ge 1 ]] || usage
        config_path="$(resolve_config "${POS_ARGS[0]}")"
        port="${POS_ARGS[1]:-}"
        if [[ ! -f "$config_path" ]]; then
            echo "error: config file not found: $config_path" >&2
            exit 1
        fi
        ESP32_EPD_CONFIG="$config_path" "$PIO" run -e "$ENV_SELECTION" -t upload ${port:+--upload-port "$port"}
        ESP32_EPD_CONFIG="$config_path" "$PIO" device monitor ${port:+-p "$port"} "${POS_ARGS[@]:2}"
        ;;
    run)
        "$PIO" "${@:2}"
        ;;
    *)
        usage
        ;;
esac

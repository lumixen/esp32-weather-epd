# AGENTS.md

ESP32 e-paper weather display (PlatformIO, pioarduino platform: Arduino core 3.3.11 compiled as an ESP-IDF 5.5.5 component).

## Build

PlatformIO is NOT on `PATH`; use the project virtualenv binary:

```sh
~/.platformio/penv/bin/pio run -e lolin_d32
```

- Single environment: `lolin_d32` (board `lolin_d32`, framework `arduino, espidf`, ESP32 @ 80 MHz).
- Verify: build must end with `[SUCCESS]`. Typical footprint: RAM ~33% (106 KB / 320 KB), Flash ~47% (1.5 MB / 3 MB).
- Upload to device: `~/.platformio/penv/bin/pio run -e lolin_d32 -t upload`; serial monitor: `-t monitor` (115200 baud).

## ESP-IDF configuration

- The platform (`framework = arduino, espidf`) builds Arduino as an ESP-IDF component, so ESP-IDF options are configurable.
- `sdkconfig.defaults` is the source of truth for the ESP-IDF side (CPU frequency, flash size, partition table, mbedTLS options, Arduino autostart/variant); the full expanded config lands in the generated `sdkconfig.<env>` (build artifact).
- Change options in `sdkconfig.defaults`, or interactively with `~/.platformio/penv/bin/pio run -e lolin_d32 -t menuconfig`.
- Gotchas: `CONFIG_MBEDTLS_PSK_MODES=y` + `CONFIG_MBEDTLS_KEY_EXCHANGE_PSK=y` are required by Arduino 3.x `NetworkClientSecure` (its `ssl_client.cpp` compiles out otherwise → undefined references at link). `CONFIG_ARDUINO_VARIANT="d32"` supplies board-specific defines (`LED_BUILTIN` etc.).

## Testing

Unit tests run on the ESP32 QEMU emulator inside Docker — no hardware needed:

```sh
bash test/run_tests_docker.sh
```

- Suite: `lolin_d32_qemu` env (Unity); the run must end with all test cases succeeding.
- The env compiles `src/` together with the tests (`test_build_src = yes`); `main.cpp` is excluded via `#if !defined(PIO_UNIT_TESTING)`. New suites: create `test/src/<suite>/test_<suite>.cpp`; `test_dir = test/src` keeps the rest of `test/` (Docker/QEMU infra) out of the build.
- `PIO_UNIT_TESTING` is set via `build_src_flags` in the qemu env: pioarduino compiles `src/` through the IDF build, which does not receive the macro PlatformIO's test runner adds for libraries.
- Mechanics: repo bind-mounted read-write at `/project` → build artifacts land in `.pio/` on the host. PlatformIO toolchains are cached in the named Docker volume `esp32-weather-epd-pio` (`/root/.platformio` in-container; `docker volume rm esp32-weather-epd-pio` forces re-download). QEMU is baked into the image at `/opt/qemu`; the `esp32-weather-epd-test` image is rebuilt only when missing (`docker rmi` to force).
- The broker uses the PlatformIO venv's esptool (`~/.platformio/penv/bin/esptool`): the tool-esptoolpy package ships esptool v5, whose Python deps the container image does not provide.

## Configuration pipeline

- `config.yml` is the user configuration source of truth (WiFi, API keys, units, layout, colors, locale).
- `scripts/config.py` (registered as `extra_scripts`) validates `config.yml` against `scripts/schema.py` (pydantic) and generates `include/config.h` at build time. The header holds compile-time selection macros (`EPD_PANEL_*`, `UNITS_*`, `BME_TYPE_*`, `COLORS_*`, ...) plus typed constants (`inline constexpr` for numbers/strings, `inline const String` for Arduino Strings — C++17 `inline` keeps the header ODR-safe). The old hand-written `config.h` + `config.cpp` and the generated `defines.h` are merged into this one file; non-user-configurable values (endpoints, battery thresholds, HTTP timeout, `NVS_NAMESPACE`) are baked in by the generator.
- **Never edit `include/config.h` manually** — it is auto-generated (gitignored); change `config.yml` instead. Config changes take effect on the next build.
- Layout macros like `POS_MOONRISE` come from `leftPanelLayout` in `config.yml`.

### Multiple devices (`devices/`)

- Per-device configs live in `devices/<name>.yml` (gitignored, may contain credentials; only `*.example.yml` templates are tracked). Each is a fully independent config (panel, pins, WiFi, location, MQTT) validated against the same schema. `config.yml` remains the default for plain `pio run` and the QEMU test env.
- The build picks the config via the `ESP32_EPD_CONFIG` env var: a bare name → `devices/<name>.yml`; anything with a path separator → used as a path; unset → `config.yml`. `scripts/config.py` resolves this, fails fast on missing/invalid files, and emits `CONFIG_SOURCE`/`CONFIG_DEVICE_NAME` into the header.
- `scripts/devices.sh` wraps pio (finds it via `PIO_BIN` → `PATH` → `~/.platformio/penv/bin/pio`): `./scripts/devices.sh flash <name>` (also `build`, `monitor`, `validate`, `list`, `list-envs`; `--env <name>` selects the platformio.ini env, default `lolin_d32`).
- `scripts/config.py` can run standalone for validation without building: `python scripts/config.py --validate <name|path>`.

## Source layout

- `src/` — implementation (`main.cpp`, `renderer.cpp`, `display_utils.cpp`, weather/air-quality/alert providers, `moon_tools.cpp`, `time_utils.cpp`).
- `include/` — headers and data models (`data_models.h` = provider-agnostic forecast models, `config.h` = generated config, `renderer.h`, `display_utils.h`, `moon_tools.h`, `time_utils.h`).
- `lib/` — custom local libraries.
- `icons/`, `fonts/` — asset generation tooling (SVG → header pipelines), not part of the build.
- Moon rise/set/phase are computed locally (`moon_tools.cpp`, libs `MoonRise` + `moonPhase-esp32`) and passed to the renderer as a standalone `moon_state_t`; providers do not supply moon data.

## Conventions

- Files carry the GPL-3.0 header (see existing files). Match the existing code style when editing.
- Constants use `UPPER_SNAKE_CASE` (e.g. `NTP_SYNC_INTERVAL_WAKEUPS`, `RTC_DRIFT_LEARN_ALPHA`); types, functions and variables use camelCase. Prefer a domain prefix (`RTC_DRIFT_*`, `HTTP_CLIENT_*`) over bare generic names.

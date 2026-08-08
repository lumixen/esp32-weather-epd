# AGENTS.md

ESP32 e-paper weather display (PlatformIO / ESP-IDF Arduino framework).

## Build

PlatformIO is NOT on `PATH`; use the project virtualenv binary:

```sh
cd platformio
~/.platformio/penv/bin/pio run -e lolin_d32
```

- Single environment: `lolin_d32` (board `lolin_d32`, Arduino framework, ESP32 @ 80 MHz).
- Verify: build must end with `[SUCCESS]`. Typical footprint: RAM ~31% (102 KB / 320 KB), Flash ~47% (1.48 MB / 3 MB).
- Upload to device: `~/.platformio/penv/bin/pio run -e lolin_d32 -t upload`; serial monitor: `-t monitor` (115200 baud).

## Testing

Unit tests run on the ESP32 QEMU emulator inside Docker — no hardware needed:

```sh
cd platformio
bash test/run_tests_docker.sh
```

- Suite: `lolin_d32_qemu` env; tests live in `test/src/test_meteoalarm/` (Unity); the run must end with all test cases succeeding.
- The env compiles `src/` together with the tests (`test_build_src = yes`); `main.cpp` is excluded via `#if !defined(PIO_UNIT_TESTING)`. New suites: create `test/src/<suite>/test_<suite>.cpp`; `test_dir = test/src` keeps the rest of `test/` (Docker/QEMU infra) out of the build.
- Mechanics: repo bind-mounted read-write at `/project` → build artifacts land in `platformio/.pio/` on the host. PlatformIO toolchains are cached in the named Docker volume `esp32-weather-epd-pio` (`/root/.platformio` in-container; `docker volume rm esp32-weather-epd-pio` forces re-download). QEMU is baked into the image at `/opt/qemu`; the `esp32-weather-epd-test` image is rebuilt only when missing (`docker rmi` to force).

## Configuration pipeline

- `platformio/config.yml` is the user configuration source of truth (WiFi, API keys, units, layout, colors, locale).
- `scripts/config.py` (registered as `extra_scripts`) validates `config.yml` against `scripts/schema.py` (pydantic) and generates `include/defines.h` at build time.
- **Never edit `include/defines.h` manually** — it is auto-generated; change `config.yml` instead. Config changes take effect on the next build.
- Layout macros like `POS_MOONRISE` come from `leftPanelLayout` in `config.yml`.

## Source layout

- `src/` — implementation (`main.cpp`, `renderer.cpp`, `display_utils.cpp`, weather/air-quality/alert providers, `moon_tools.cpp`).
- `include/` — headers and data models (`data_models.h` = provider-agnostic forecast models, `defines.h` = generated config, `renderer.h`, `display_utils.h`, `moon_tools.h`).
- `lib/` — custom local libraries.
- `icons/`, `fonts/` — asset generation tooling (SVG → header pipelines), not part of the build.
- Moon rise/set/phase are computed locally (`moon_tools.cpp`, libs `MoonRise` + `moonPhase-esp32`) and passed to the renderer as a standalone `moon_state_t`; providers do not supply moon data.

## Conventions

- Files carry the GPL-3.0 header (see existing files). Match the existing code style when editing.

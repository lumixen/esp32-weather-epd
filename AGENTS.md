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

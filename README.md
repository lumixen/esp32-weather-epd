# ESP32 E-Paper Weather Display

[![Build](https://github.com/lumixen/esp32-weather-epd/actions/workflows/build.yaml/badge.svg)](https://github.com/lumixen/esp32-weather-epd/actions/workflows/build.yaml)
[![Tests](https://github.com/lumixen/esp32-weather-epd/actions/workflows/test.yaml/badge.svg)](https://github.com/lumixen/esp32-weather-epd/actions/workflows/test.yaml)

Weather display firmware for the Lolin D32 and supported 7-inch e-paper
panels. It combines weather forecasts, air quality, alerts, optional local
sensor measurements, and Home Assistant MQTT discovery in a low-power display
that periodically wakes, refreshes, and sleeps.

## Contents

- [Features](#features)
- [Supported panels](#supported-panels)
- [Quick start](#quick-start)
- [Hardware and wiring](#hardware-and-wiring)
- [Everything is a provider](#everything-is-a-provider)
- [Configuration](#configuration)
- [Multiple devices](#multiple-devices)
- [Home Assistant integration](#home-assistant-integration)
- [QEMU unit tests](#qemu-unit-tests)
- [Troubleshooting](#troubleshooting)
- [Contributing and license](#contributing-and-license)

## Features

- Forecasts, air-quality data, and weather alerts from multiple providers.
- Optional local sensor measurements.
- Support for black-and-white, red/black/white, and 7-color e-paper panels.
- Home Assistant integration through MQTT auto-discovery.
- Independent configuration for multiple displays.

<p float="left">
  <img src="https://github.com/user-attachments/assets/15d49106-3b07-4fbe-b252-1a642cb1251c" width="49%" />
  <img src="https://github.com/user-attachments/assets/d7e9c4d2-8885-43cd-aa22-df5e04820dd2" width="49%" />
</p>

Enclosure files and assembly instructions are available on
[Printables](https://www.printables.com/model/1469770-75-e-paper-frame-for-lolin-d32-waveshare-driver).

## Supported panels

The firmware is set up for a Lolin D32 connected to an e-paper driver board.
Select the panel and driver that match your hardware in the configuration.

| Panel | Resolution | Colors | Status |
|---|---:|---|---|
| Waveshare 7.5-inch e-paper (v2) | 800x480 | Black/White | Supported |
| Good Display GDEY075T7, 7.5-inch | 800x480 | Black/White | Supported |
| Waveshare 7.5-inch e-Paper (B) | 800x480 | Red/Black/White | Supported |
| Good Display GDEY075Z08, 7.5-inch | 800x480 | Red/Black/White | Supported |
| Waveshare 7.3-inch ACeP e-Paper (F) | 800x480 | 7-color | Supported |
| Good Display GDEY073D46, 7.3-inch | 800x480 | 7-color | Supported |
| Waveshare 7.5-inch e-paper (v1) | 640x384 | Black/White | Limited support; some information is not displayed |
| Good Display GDEW075T8, 7.5-inch | 640x384 | Black/White | Limited support; some information is not displayed |
| DKE DEPG0750RWF86BF (86BF), 7.5-inch | 800x480 | Red/Black/White | Supported |

## Quick start

### Prerequisites

- A Lolin D32 ESP32 board.
- A supported e-paper panel and compatible driver board.
- A USB cable suitable for programming the board.
- [PlatformIO](https://platformio.org/), either through the PlatformIO IDE
  extension for VS Code or the PlatformIO CLI.
- Network access for the configured weather providers and, optionally, an MQTT
  broker.

Docker is additionally required only for the QEMU unit tests.

### Configure, build, and upload

1. Clone this repository and open it in VS Code, or change into the repository
   directory in a terminal.
2. Create the local configuration from the tracked example:

   ```sh
   cp config.example.yml config.yml
   ```

3. Edit `config.yml` for your panel, driver, WiFi network, location, timezone,
   providers, and display preferences. The file is ignored by Git because it
   can contain credentials.
4. Validate the configuration before building:

   ```sh
   ~/.platformio/penv/bin/python scripts/config.py --validate ./config.yml
   ```

5. Build the default PlatformIO environment:

   ```sh
   ~/.platformio/penv/bin/pio run -e lolin_d32
   ```

6. Connect the board over USB and upload the firmware:

   ```sh
   ~/.platformio/penv/bin/pio run -e lolin_d32 -t upload
   ```

   If `pio` is on your `PATH`, the `~/.platformio/penv/bin/` prefix is not
   needed. The same build and upload operations are available from the
   PlatformIO buttons in VS Code.

7. View serial output at 115200 baud when troubleshooting:

   ```sh
   ~/.platformio/penv/bin/pio device monitor -b 115200
   ```

The default environment is `lolin_d32`. The generated
`include/config.h` is a build artifact; do not edit it manually. Change
`config.yml` and build again instead.

## Hardware and wiring

The following wiring is specific to the Lolin D32 board. Verify the pinout and
voltage requirements of the selected driver board before connecting it.

<table>
  <tr>
    <td valign="middle">
      <img width="459" alt="Lolin D32 wiring schematic" src="https://github.com/user-attachments/assets/278b804c-fa89-4595-b60a-8fa0e6571931" />
    </td>
    <td valign="top">
      <table>
        <thead>
          <tr>
            <th>E-paper pin</th>
            <th>Lolin D32 pin</th>
          </tr>
        </thead>
        <tbody>
          <tr><td>PWR</td><td>GPIO2</td></tr>
          <tr><td>BUSY</td><td>GPIO4</td></tr>
          <tr><td>RST</td><td>GPIO16</td></tr>
          <tr><td>DC</td><td>GPIO17</td></tr>
          <tr><td>CS</td><td>GPIO5</td></tr>
          <tr><td>CLK</td><td>GPIO18</td></tr>
          <tr><td>DIN</td><td>GPIO23</td></tr>
          <tr><td>VCC</td><td>3V3</td></tr>
        </tbody>
      </table>
    </td>
  </tr>
</table>

The pin assignments are configurable where supported by the hardware. The
full example includes the `pin` section and is the reference for optional
pins such as the battery ADC and BME280 connections.

## Everything is a provider

The `providers` list is the central data-integration model. A provider is not
limited to a remote weather API: it is any component that supplies one or more
named data groups to the display's provider-agnostic weather report.

Each configured provider owns one or more capabilities:

| Capability | Meaning |
|---|---|
| `current_forecast` | Current weather conditions |
| `hourly_forecast` | Hourly forecast data |
| `daily_forecast` | Daily forecast data |
| `air_quality` | Air-quality measurements and index data |
| `alerts` | Weather alerts |
| `in_temperature` | Local indoor temperature |
| `in_humidity` | Local indoor humidity |
| `in_pressure` | Local indoor pressure |

Remote providers fetch and normalize data from an external service. Local
providers, such as `bme280`, use the same model even though they read a sensor
instead of making an HTTP request. The renderer consumes the normalized report
and does not need to know which API or sensor produced the data.

A provider may own multiple capabilities. For example, OpenWeatherMap One
Call provides forecast data and alerts. Providers may also create more than
one fetch operation when a service uses separate endpoints internally.

Each capability can have at most one owner. The current, hourly, and daily
forecast capabilities are required. Air quality is required when `AIR_QUALITY`
is present in `rendering.leftPanelLayout`. Alerts and local sensor capabilities are
optional. The configuration validator rejects unknown providers, duplicate
capability ownership, missing required capabilities, and unsupported
precipitation settings.

Provider-specific settings stay on the provider entry. Depending on the
provider, these can include `transport`, `apiKey`, `alerts`, `country`,
`forecastPointId`, `stationId`, and BME280 pin and address settings.

For example, these entries combine into one report:

```yaml
providers:
  - provider: open_meteo_forecast       # current/hourly/daily forecast
    transport: HTTPS_VERIFY
  - provider: open_meteo_air_quality    # air quality
    transport: HTTPS_VERIFY
  - provider: meteoalarm_alert           # alerts
    country: netherlands
  - provider: bme280                     # indoor temperature/humidity/pressure
    pins:
      power: 27
      sda: 21
      scl: 22
    address: 0x76
```

Normally, combine complementary providers rather than configuring two
providers for the same capability. The authoritative capability declarations
are in [`scripts/provider_capabilities.py`](scripts/provider_capabilities.py).

### Available providers

| Provider | Data |
|---|---|
| [Open-Meteo](https://open-meteo.com/)<br>`open_meteo_forecast` | `current_forecast`, `hourly_forecast`, `daily_forecast` |
| [Open-Meteo Air Quality](https://open-meteo.com/en/docs/air-quality-api)<br>`open_meteo_air_quality` | `air_quality` |
| [NOAA/NWS](https://www.weather.gov/documentation/services-web-api)<br>`noaa_forecast` | `current_forecast`, `hourly_forecast`, `daily_forecast` |
| [MeteoSwiss](https://www.meteoswiss.admin.ch/)<br>`meteoswiss_forecast` | `current_forecast`, `hourly_forecast`, `daily_forecast` |
| [OpenWeatherMap One Call 3.0](https://openweathermap.org/api/one-call-3)<br>`openweathermap_onecall_v3` | `current_forecast`, `hourly_forecast`, `daily_forecast`, `alerts` (embedded alerts) |
| [OpenWeatherMap One Call 4.0](https://openweathermap.org/api/one-call-4)<br>`openweathermap_onecall_v4` | `current_forecast`, `hourly_forecast`, `daily_forecast` (current, hourly, and daily timelines); optional `alerts` with `alerts: true` |
| [OpenWeatherMap Air Pollution](https://openweathermap.org/api/air-pollution)<br>`openweathermap_air_quality` | `air_quality` |
| [MeteoAlarm](https://www.meteoalarm.org/)<br>`meteoalarm_alert` | `alerts` |
| [BME280](https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/)<br>`bme280` | `in_temperature`, `in_humidity`, `in_pressure` |

## Configuration

[`config.example.yml`](config.example.yml) is the canonical full configuration
example. Copy it to `config.yml` and edit it rather than copying a large YAML
snippet from this README. Per-device installations use the separate
[`devices/kitchen.example.yml`](devices/kitchen.example.yml) template.

The main configuration groups are:

| Group | Purpose |
|---|---|
| `display` | E-paper panel, driver board, and display pins |
| `providers` | Select forecast, air-quality, alert, and local sensor sources |
| `wifi` | WiFi credentials and optional network settings |
| `location` | Coordinates, city, and timezone |
| `ntp` | NTP servers, synchronization interval, and timeout |
| `rendering.units` | Temperature, speed, pressure, distance, and precipitation units |
| `rendering` | Fonts, formatting, layout, status-bar options, and colors |
| `battery` | Battery monitoring and ADC pin |
| `schedule` | Refresh and sleep behavior |
| `homeAssistantMqtt` | Optional Home Assistant MQTT integration |
| `logLevel` | Serial logging verbosity |

`rendering.leftPanelLayout` maps one-based display slots to items. Missing
slots are left empty, so sparse layouts are supported:

```yaml
rendering:
  leftPanelLayout:
    1: SUNRISE
    2: SUNSET
    5: HUMIDITY
    6: WIND
```

The complete list of valid values and validation rules is defined in
[`scripts/schema.py`](scripts/schema.py).

## Multiple devices

The firmware is compiled separately for each device. Every device can have an
independent panel, pinout, WiFi network, location, provider list, MQTT setup,
and display layout.

Device configurations live in `devices/<name>.yml`. They are ignored by Git
because they contain credentials; only `*.example.yml` templates are tracked.
Start with the template:

```sh
cp devices/kitchen.example.yml devices/kitchen.yml
./scripts/devices.sh validate kitchen
./scripts/devices.sh build kitchen
./scripts/devices.sh flash kitchen
```

The wrapper locates PlatformIO through `PIO_BIN`, `PATH`, or
`~/.platformio/penv/bin/pio` and supports these commands:

```sh
./scripts/devices.sh list
./scripts/devices.sh validate kitchen
./scripts/devices.sh build kitchen
./scripts/devices.sh flash kitchen
./scripts/devices.sh flash kitchen /dev/ttyUSB0
./scripts/devices.sh monitor kitchen
./scripts/devices.sh flash-monitor kitchen
./scripts/devices.sh build kitchen --env lolin_d32_qemu
./scripts/devices.sh list-envs
```

Without the wrapper, select a configuration with `ESP32_EPD_CONFIG`. A bare
name is resolved as `devices/<name>.yml`; a path containing a separator is used
as a path directly:

```sh
ESP32_EPD_CONFIG=kitchen ~/.platformio/penv/bin/pio run -e lolin_d32 -t upload
```

When `ESP32_EPD_CONFIG` is unset, the regular environment uses the root
`config.yml`. The `lolin_d32_qemu` environment instead uses the committed
configuration marked `default` in [`test/configs/index.yml`](test/configs/index.yml).

## Home Assistant integration

The optional MQTT integration publishes device information and sensor values
using Home Assistant's MQTT discovery protocol. Enable it in the selected
configuration:

```yaml
homeAssistantMqtt:
  enabled: true
  server: 192.168.1.10
  port: 1883
  username: esp32_weather
  password: your-mqtt-password
  deviceName: Kitchen weather display
  discoveryPrefix: homeassistant
```

The integration can publish:

- Battery level
- Battery voltage
- API activity duration
- WiFi signal strength
- Temperature
- Humidity
- Pressure

Battery values require battery monitoring. Temperature, humidity, and pressure
are published when the corresponding data is available from the configured
providers or local sensor. The default state-topic prefix is
`esp32_weather_epd/<clientId>/`; discovery topics use the configured
`discoveryPrefix`.

## QEMU unit tests

The unit tests run inside Docker using an ESP32 QEMU emulator and do not
require physical hardware. Test configurations are committed and pinned under
[`test/configs/`](test/configs/); they do not use the untracked local
`config.yml`.

List available configurations:

```sh
bash test/run_tests_docker.sh --list
```

Run all registered configurations:

```sh
bash test/run_tests_docker.sh
```

Run one configuration while developing, or select a subset:

```sh
bash test/run_tests_docker.sh --config owm3 -v
bash test/run_tests_docker.sh --config openmeteo --config noaa
```

Each selected configuration gets its own compatible build and QEMU boot. A
new test configuration requires a pinned YAML file under `test/configs/`, a
matching `test/src/test_<id>/` suite, and an entry in the registry. The runner
and CI matrix discover registered configurations automatically.

## Troubleshooting

### Configuration validation fails

Run the validator directly and read the reported field or capability error:

```sh
~/.platformio/penv/bin/python scripts/config.py --validate ./config.yml
```

Check that exactly one provider owns the forecast capabilities and that a
provider for `air_quality` is configured when `AIR_QUALITY` is used in the
layout.

### PlatformIO is not found

Install PlatformIO through VS Code or the CLI. If it is installed in the
standard PlatformIO virtual environment, use
`~/.platformio/penv/bin/pio`, or set `PIO_BIN` when using `scripts/devices.sh`.

### Upload fails or the port is not detected

Confirm that the board is connected, select the correct USB serial port, and
pass it explicitly:

```sh
~/.platformio/penv/bin/pio run -e lolin_d32 -t upload --upload-port /dev/ttyUSB0
./scripts/devices.sh flash kitchen /dev/ttyUSB0
```

The device wrapper also accepts the corresponding port for `monitor` and
`flash-monitor`.

### WiFi, NTP, or API data is unavailable

Verify the WiFi credentials, location, timezone, provider configuration, and
serial logs. Increase the NTP timeout if synchronization fails on a slow
network. For HTTPS failures, check the selected transport and the device time.

### MQTT is not discovered by Home Assistant

Confirm that the broker address, port, username, password, and
`discoveryPrefix` are correct. Check serial output for MQTT connection errors
and ensure the broker is reachable from the device's network.

### QEMU tests fail

Ensure Docker is running and retry the selected configuration with verbose
output:

```sh
bash test/run_tests_docker.sh --config openmeteo -v
```

The test runner saves QEMU diagnostics under `.pio/build/lolin_d32_qemu/`.

## Contributing and license

See [`AGENTS.md`](AGENTS.md) for repository build, testing, formatting, and
configuration conventions. The project is licensed under the
[GNU General Public License v3.0](LICENSE).

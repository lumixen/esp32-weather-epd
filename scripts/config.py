# Configuration validation and header generation for PlatformIO.
# Copyright (C) 2025  Matteo Battistutta
# Copyright (C) 2026  Max Bodaniuk
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

try:
    Import("env")  # noqa: F821 - provided by the PlatformIO build system
except NameError:
    env = None

import os
import sys

try:
    _script_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    _script_dir = None
if _script_dir is not None:
    sys.path.insert(0, _script_dir)

try:
    import pydantic
except ImportError:
    pydantic = None
try:
    import yaml
except ImportError:
    yaml = None

if env is not None:
    if pydantic is None:
        env.Execute("$PYTHONEXE -m pip install pydantic")
        import pydantic
    if yaml is None:
        env.Execute("$PYTHONEXE -m pip install pyyaml")
        import yaml

if pydantic is None:
    raise SystemExit("pydantic is required: pip install pydantic")
if yaml is None:
    raise SystemExit("pyyaml is required: pip install pyyaml")

from pydantic import ValidationError
from schema import ConfigSchema, Color
from re import sub
from datetime import datetime


def upper_snake(s: str):
    return "_".join(
        sub("([A-Z][a-z]+)", r" \1", sub("([A-Z]+)", r" \1", s)).split()
    ).upper()


def escape_c_string(value):
    """Escape a string for C++ string literal."""
    s = str(value)
    s = s.replace("\\", "\\\\")
    s = s.replace('"', '\\"')
    s = s.replace("\n", "\\n")
    s = s.replace("\r", "\\r")
    return s


def format_bssid(bssid_str):
    """Convert BSSID string to C++ uint8_t array initializer."""
    # Remove colon separators and convert to uppercase
    cleaned = bssid_str.replace(":", "").upper()

    # Split into pairs of hex digits
    hex_pairs = [cleaned[i : i + 2] for i in range(0, len(cleaned), 2)]

    # Format as C++ array initializer
    formatted = ", ".join([f"0x{pair}" for pair in hex_pairs])
    return f"{{{formatted}}}"


# Numeric value per log level name, matching the LogLevel enum in logger.h.
_LOG_LEVEL_NUMBERS = {
    "TRACE": 0,
    "DEBUG": 1,
    "INFO": 2,
    "WARNING": 3,
    "ERROR": 4,
    "CRITICAL": 5,
}


# C++ types emitted for runtime configuration values.
#
# Values that only take part in preprocessor conditionals (#if / #ifdef /
# token pasting / string literal concatenation) are emitted as #define macros
# (`EPD_PANEL_*`, `LOCALE`, `LOG_LEVEL`, `COLORS_*`, ...). Every other value
# becomes a typed constant: `inline constexpr` for plain data (C++17 `inline`
# keeps the definitions ODR-safe in the header), `inline const String` where
# consumers need an Arduino String.
STRING = "const char *"

# Name overrides (generated macro-name derivation differs from the established
# constant names).
NAME_OVERRIDES = {
    "latitude": "LAT",
    "longitude": "LON",
    "city": "CITY_STRING",
}

# C++ type per constant name; unspecified names default to `int` (numbers)
# or `const char *` (strings).
TYPED_TYPES = {
    # device/config identity
    "CONFIG_SOURCE": STRING,
    "CONFIG_DEVICE_NAME": STRING,
    # ntp
    "NTP_SERVER_1": STRING,
    "NTP_SERVER_2": STRING,
    "NTP_SYNC_INTERVAL_WAKEUPS": "int",
    "NTP_TIMEOUT": "int",
    # bme
    "BME_PIN_PWR": "int",
    "BME_PIN_SDA": "int",
    "BME_PIN_SCL": "int",
    "BME_ADDRESS": "uint8_t",
    # pin
    "PIN_BAT_ADC": "int",
    "PIN_EPD_BUSY": "int",
    "PIN_EPD_CS": "int",
    "PIN_EPD_RST": "int",
    "PIN_EPD_DC": "int",
    "PIN_EPD_SCK": "int",
    "PIN_EPD_MISO": "int",
    "PIN_EPD_MOSI": "int",
    "PIN_EPD_PWR": "int",
    # wifi
    "WIFI_SSID": STRING,
    "WIFI_PASSWORD": STRING,
    "WIFI_TIMEOUT": "unsigned long",
    "WIFI_STATIC_IP_IP": STRING,
    "WIFI_STATIC_IP_GATEWAY": STRING,
    "WIFI_STATIC_IP_SUBNET": STRING,
    "WIFI_STATIC_IP_DNS1": STRING,
    "WIFI_STATIC_IP_DNS2": STRING,
    # owm
    "OWM_APIKEY": "String",
    # alerts
    "METEOALARM_COUNTRY": "String",
    # location
    "LAT": "String",
    "LON": "String",
    "CITY_STRING": "String",
    # time
    "TIMEZONE": STRING,
    "TIME_FORMAT": STRING,
    "HOUR_FORMAT": STRING,
    "DATE_FORMAT": STRING,
    "REFRESH_TIME_FORMAT": STRING,
    # sleep
    "SLEEP_DURATION": "int",
    "BED_TIME": "int",
    "WAKE_TIME": "int",
    "HOURLY_GRAPH_MAX": "int",
    # home assistant MQTT
    "HOME_ASSISTANT_MQTT_SERVER": STRING,
    "HOME_ASSISTANT_MQTT_PORT": "uint16_t",
    "HOME_ASSISTANT_MQTT_USERNAME": STRING,
    "HOME_ASSISTANT_MQTT_PASSWORD": STRING,
    # colors (numeric thresholds; color tokens remain COLORS_* macros)
    "COLORS_OUTLOOK_LOW_THRESHOLD_TEMPERATURE": "int",
    "COLORS_OUTLOOK_HIGH_THRESHOLD_TEMPERATURE": "int",
}

# Constant values that are not user-configurable: they are baked into the
# generated header with their proper C++ types.
INTERNAL_CONSTANTS = [
    ("HTTP_CLIENT_TCP_TIMEOUT", "uint32_t", 2000),      # ms
    ("OWM_ENDPOINT", "String", "api.openweathermap.org"),
    ("OM_ENDPOINT", "String", "api.open-meteo.com"),
    ("OM_AIR_QUALITY_ENDPOINT", "String", "air-quality-api.open-meteo.com"),
    ("WARN_BATTERY_VOLTAGE", "uint32_t", 3535),          # (millivolts) ~20%
    ("LOW_BATTERY_VOLTAGE", "uint32_t", 3462),           # (millivolts) ~10%
    ("VERY_LOW_BATTERY_VOLTAGE", "uint32_t", 3442),      # (millivolts)  ~8%
    ("CRIT_LOW_BATTERY_VOLTAGE", "uint32_t", 3404),      # (millivolts)  ~5%
    ("LOW_BATTERY_SLEEP_INTERVAL", "unsigned long", 30),        # (minutes)
    ("VERY_LOW_BATTERY_SLEEP_INTERVAL", "unsigned long", 120),  # (minutes)
    ("MAX_BATTERY_VOLTAGE", "uint32_t", 4200),           # (millivolts)
    ("MIN_BATTERY_VOLTAGE", "uint32_t", 3000),           # (millivolts)
]

# Font name to header mappings
FONT_FILES = {
    "FreeMono": "fonts/FreeMono.h",
    "FreeSans": "fonts/FreeSans.h",
    "FreeSerif": "fonts/FreeSerif.h",
    "Lato": "fonts/Lato_Regular.h",
    "Montserrat": "fonts/Montserrat_Regular.h",
    "Open Sans": "fonts/OpenSans_Regular.h",
    "Poppins": "fonts/Poppins_Regular.h",
    "Quicksand": "fonts/Quicksand_Regular.h",
    "Raleway": "fonts/Raleway_Regular.h",
    "Roboto": "fonts/Roboto_Regular.h",
    "Roboto Mono": "fonts/RobotoMono_Regular.h",
    "Roboto Slab": "fonts/RobotoSlab_Regular.h",
    "Ubuntu": "fonts/Ubuntu_R.h",
    "Ubuntu Mono": "fonts/UbuntuMono_R.h",
}


def constant_name(key):
    """Derive the constant/macro name for a config.yml key, applying
    established name overrides."""
    return NAME_OVERRIDES.get(key, upper_snake(key))


def typed_constant_line(name, cpp_type, value):
    """Format a typed constant declaration for the C++ header."""
    if value is None:
        value = ""
    if cpp_type == "String":
        return f'inline const String {name} = "{escape_c_string(value)}";'
    if cpp_type == STRING:
        return f'inline constexpr const char *{name} = "{escape_c_string(value)}";'
    return f"inline constexpr {cpp_type} {name} = {value};"


def emit_typed(lines, name, value):
    """Emit a typed constant, choosing the C++ type from TYPED_TYPES."""
    cpp_type = TYPED_TYPES.get(name, STRING if isinstance(value, str) else "int")
    lines.append(typed_constant_line(name, cpp_type, value))


def emit_define(lines, name, value=None):
    lines.append(f"#define {name}" if value is None else f"#define {name} {value}")


def resolve_config_path(value=None):
    """Resolve the config file from ESP32_EPD_CONFIG (or an explicit value).

    A bare device name (no path separator) is looked up in devices/; anything
    else is treated as a path. Falls back to config.yml.
    """
    if value is None:
        value = os.environ.get("ESP32_EPD_CONFIG")
    if not value:
        return "config.yml"
    if "/" not in value and os.sep not in value:
        return os.path.join("devices", f"{value}.yml")
    return value


def device_name(config_path):
    """Derive the device name from the config file path (file stem)."""
    return os.path.splitext(os.path.basename(config_path))[0]


def generate(config_path, header_path, write_header=True):
    """Validate a config file and generate the C++ configuration header."""
    if not os.path.isfile(config_path):
        raise SystemExit(
            f"Configuration file not found: {config_path} "
            "(set ESP32_EPD_CONFIG to a device name or path, or use config.yml)"
        )

    with open(config_path, "r", encoding="utf-8") as config_file:
        try:
            user_config = yaml.safe_load(config_file)
        except yaml.YAMLError as exc:
            raise SystemExit(f"Invalid configuration in {config_path}:\n{exc}") from exc
    if not isinstance(user_config, dict):
        got = type(user_config).__name__ if user_config is not None else "empty file"
        raise SystemExit(f"Invalid configuration in {config_path}: expected a YAML mapping, got {got}")
    try:
        config = ConfigSchema(**user_config)
    except ValidationError as exc:
        raise SystemExit(f"Invalid configuration in {config_path}:\n{exc}") from exc

    # Generate header file
    header_lines = [
        "// Auto-generated configuration header",
        f"// DO NOT EDIT - Generated from {config_path}",
        "",
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "#include <cstdint>",
        "",
    ]

    # Add build version with current date/time
    build_version = datetime.now().strftime("%Y.%m.%d %H:%M")
    header_lines.append("// Build Information")
    emit_define(header_lines, "BUILD_VERSION", f'"{build_version}"')
    emit_typed(header_lines, "CONFIG_SOURCE", config_path)
    emit_typed(header_lines, "CONFIG_DEVICE_NAME", device_name(config_path))
    header_lines.append("")

    # E-Paper display and locale
    header_lines.append("// Configuration")
    emit_define(header_lines, f"EPD_PANEL_{config.epdPanel.name}")
    emit_define(header_lines, f"EPD_DRIVER_{config.epdDriver.name}")
    # LOCALE is a token-pasting target in locale.cpp (no quotes)
    emit_define(header_lines, "LOCALE", config.locale.value)

    # weatherAPI configuration
    header_lines.append("// weatherAPI configuration")
    emit_define(header_lines, f"WEATHER_API_PROVIDER_{config.weatherAPI.provider.name}")
    emit_define(header_lines, f"WEATHER_API_TRANSPORT_{config.weatherAPI.transport.name}")

    # airQualityAPI configuration
    header_lines.append("// airQualityAPI configuration")
    emit_define(header_lines, f"AIR_QUALITY_API_PROVIDER_{config.airQualityAPI.provider.name}")
    emit_define(header_lines, f"AIR_QUALITY_API_TRANSPORT_{config.airQualityAPI.transport.name}")

    # ntp configuration
    header_lines.append("// ntp configuration")
    emit_typed(header_lines, "NTP_SERVER_1", config.ntp.server_1)
    emit_typed(header_lines, "NTP_SERVER_2", config.ntp.server_2)
    emit_typed(header_lines, "NTP_SYNC_INTERVAL_WAKEUPS", config.ntp.syncIntervalWakeups)
    emit_typed(header_lines, "NTP_TIMEOUT", config.ntp.timeout)
    emit_define(header_lines, "RTC_DRIFT_CORRECTION", 1 if config.ntp.rtcCorrection else 0)

    # bme configuration
    header_lines.append("// bme configuration")
    emit_define(header_lines, f"BME_TYPE_{config.bme.type.upper()}")
    for key in ("pinPwr", "pinSDA", "pinSCL"):
        if hasattr(config.bme, key):
            emit_typed(header_lines, f"BME_{upper_snake(key)}", getattr(config.bme, key))
    if hasattr(config.bme, "address"):
        emit_typed(header_lines, "BME_ADDRESS", config.bme.address)
    header_lines.append("")

    # units and display configuration
    header_lines.append("// units configuration")
    emit_define(header_lines, f"UNITS_TEMP_{config.unitsTemp.name}")
    emit_define(header_lines, f"UNITS_SPEED_{config.unitsSpeed.name}")
    emit_define(header_lines, f"UNITS_PRES_{config.unitsPres.name}")
    emit_define(header_lines, f"UNITS_DISTANCE_{config.unitsDistance.name}")
    emit_define(header_lines, f"UNITS_HOURLY_PRECIP_{config.unitsHourlyPrecip.name}")
    emit_define(header_lines, f"UNITS_DAILY_PRECIP_{config.unitsDailyPrecip.name}")
    emit_define(header_lines, f"WIND_DIRECTION_INDICATOR_{config.windDirectionIndicator.name}")
    emit_define(header_lines, f"WIND_ARROW_PRECISION_{config.windArrowPrecision.name}")

    # font and display configuration
    header_lines.append("// font and display configuration")
    emit_define(header_lines, "FONT_HEADER", f'"{FONT_FILES[config.font]}"')
    emit_define(header_lines, f"DISPLAY_DAILY_PRECIP_{config.displayDailyPrecip.name}")
    emit_define(header_lines, "DISPLAY_HOURLY_ICONS", 1 if config.displayHourlyIcons else 0)

    # alertsAPI configuration
    header_lines.append("// alertsAPI configuration")
    if config.alertsAPI.provider == "None":
        emit_define(header_lines, "ALERTS_API_PROVIDER_NONE")
    elif config.alertsAPI.provider == "OpenWeatherMap":
        emit_define(header_lines, "ALERTS_API_PROVIDER_OPEN_WEATHER_MAP")
        emit_define(header_lines, f"ALERTS_API_TRANSPORT_{config.alertsAPI.transport.name}")
    else:  # MeteoAlarm
        emit_define(header_lines, "ALERTS_API_PROVIDER_METEOALARM")
        header_lines.append("#if defined(ALERTS_API_PROVIDER_METEOALARM)")
        emit_typed(header_lines, "METEOALARM_COUNTRY", config.alertsAPI.country.value)
        header_lines.append("#endif  // ALERTS_API_PROVIDER_METEOALARM")

    # status bar and debug configuration
    header_lines.append("// status bar configuration")
    emit_define(header_lines, "STATUS_BAR_EXTRAS_BAT_VOLTAGE", 1 if config.statusBarExtrasBatVoltage else 0)
    emit_define(header_lines, "STATUS_BAR_EXTRAS_WIFI_RSSI", 1 if config.statusBarExtrasWifiRSSI else 0)
    emit_define(header_lines, "BATTERY_MONITORING", 1 if config.batteryMonitoring else 0)

    # log configuration
    header_lines.append("// log configuration")
    emit_define(header_lines, f"LOG_LEVEL_{config.logLevel.name}")
    emit_define(header_lines, "LOG_LEVEL", _LOG_LEVEL_NUMBERS[config.logLevel.name])

    # pin configuration
    header_lines.append("// pin configuration")
    for key in ("batAdc", "epdBusy", "epdCS", "epdRst", "epdDC", "epdSCK", "epdMISO", "epdMOSI", "epdPwr"):
        emit_typed(header_lines, f"PIN_{upper_snake(key)}", getattr(config.pin, key))
    header_lines.append("")

    # wifi configuration
    header_lines.append("// wifi configuration")
    emit_typed(header_lines, "WIFI_SSID", config.wifi.ssid)
    emit_typed(header_lines, "WIFI_PASSWORD", config.wifi.password)
    emit_typed(header_lines, "WIFI_TIMEOUT", config.wifi.timeout)
    emit_define(header_lines, "WIFI_SCAN", 1 if config.wifi.scan else 0)
    if config.wifi.bssid is not None:
        emit_define(header_lines, "WIFI_HAS_BSSID")
        header_lines.append(
            f"inline constexpr uint8_t WIFI_BSSID[6] = {format_bssid(config.wifi.bssid)};"
        )
    if config.wifi.staticIp is not None:
        emit_define(header_lines, "WIFI_STATIC_IP_ENABLED")
        emit_typed(header_lines, "WIFI_STATIC_IP_IP", config.wifi.staticIp.ip)
        emit_typed(header_lines, "WIFI_STATIC_IP_GATEWAY", config.wifi.staticIp.gateway)
        emit_typed(header_lines, "WIFI_STATIC_IP_SUBNET", config.wifi.staticIp.subnet)
        emit_typed(header_lines, "WIFI_STATIC_IP_DNS1", config.wifi.staticIp.dns1)
        emit_typed(header_lines, "WIFI_STATIC_IP_DNS2", config.wifi.staticIp.dns2)

    # OpenWeatherMap configuration
    header_lines.append("// OpenWeatherMap configuration")
    emit_typed(header_lines, "OWM_APIKEY", config.owmApikey)

    # location configuration
    header_lines.append("// location configuration")
    emit_typed(header_lines, "LAT", config.latitude)
    emit_typed(header_lines, "LON", config.longitude)
    emit_typed(header_lines, "CITY_STRING", config.city)

    # time configuration
    header_lines.append("// time configuration")
    emit_typed(header_lines, "TIMEZONE", config.timezone)
    emit_typed(header_lines, "TIME_FORMAT", config.timeFormat)
    emit_typed(header_lines, "HOUR_FORMAT", config.hourFormat)
    emit_typed(header_lines, "DATE_FORMAT", config.dateFormat)
    emit_typed(header_lines, "REFRESH_TIME_FORMAT", config.refreshTimeFormat)

    # sleep configuration
    header_lines.append("// sleep configuration")
    emit_typed(header_lines, "SLEEP_DURATION", config.sleepDuration)
    emit_typed(header_lines, "BED_TIME", config.bedTime)
    emit_typed(header_lines, "WAKE_TIME", config.wakeTime)
    emit_typed(header_lines, "HOURLY_GRAPH_MAX", config.hourlyGraphMax)

    # homeAssistantMqtt configuration
    header_lines.append("// homeAssistantMqtt configuration")
    if config.homeAssistantMqtt is not None:
        emit_define(header_lines, "HOME_ASSISTANT_MQTT_ENABLED", 1 if config.homeAssistantMqtt.enabled else 0)
        emit_typed(header_lines, "HOME_ASSISTANT_MQTT_SERVER", config.homeAssistantMqtt.server)
        emit_typed(header_lines, "HOME_ASSISTANT_MQTT_PORT", config.homeAssistantMqtt.port)
        emit_typed(header_lines, "HOME_ASSISTANT_MQTT_USERNAME", config.homeAssistantMqtt.username)
        emit_typed(header_lines, "HOME_ASSISTANT_MQTT_PASSWORD", config.homeAssistantMqtt.password)
        # String literal concatenation targets in home_assistant_mqtt.h
        emit_define(
            header_lines,
            "HOME_ASSISTANT_MQTT_DEVICE_NAME",
            f'"{escape_c_string(config.homeAssistantMqtt.deviceName)}"',
        )
        emit_define(
            header_lines,
            "HOME_ASSISTANT_MQTT_DISCOVERY_PREFIX",
            f'"{escape_c_string(config.homeAssistantMqtt.discoveryPrefix)}"',
        )

    # leftPanelLayout configuration
    header_lines.append("// leftPanelLayout configuration")
    for name, idx in config.leftPanelLayout.items():
        emit_define(header_lines, f"POS_{name.upper()}", idx)

    # moon phase configuration
    header_lines.append("// moon phase configuration")
    emit_define(header_lines, f"MOON_PHASE_STYLE_{config.moonPhaseStyle.name}")

    # colors configuration
    header_lines.append("// colors configuration")
    for key in config.colors.model_fields:
        value = getattr(config.colors, key)
        name = f"COLORS_{upper_snake(key)}"
        if isinstance(value, Color):
            emit_define(header_lines, name, value.to_define_value())
        else:
            emit_typed(header_lines, name, value)
    header_lines.append("")

    # Internal (non-user-configurable) constants
    header_lines.append("// internal constants")
    for name, cpp_type, value in INTERNAL_CONSTANTS:
        header_lines.append(typed_constant_line(name, cpp_type, value))
    header_lines.append("")

    # NON-VOLATILE STORAGE (NVS) NAMESPACE
    header_lines.append("#ifndef NVS_NAMESPACE")
    header_lines.append('#define NVS_NAMESPACE "weather_epd"')
    header_lines.append("#endif")

    if not write_header:
        print(f"Configuration valid: {config_path}")
        return

    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    with open(header_path, "w", encoding="utf-8") as header_file:
        header_file.write("\n".join(header_lines) + "\n")

    print(f"Generated configuration header: {header_path}")
    print(f"Total defines: {len([l for l in header_lines if l.startswith('#define')])}")
    print(f"Total typed constants: {len([l for l in header_lines if l.startswith('inline const')])}")


if env is not None:
    # PlatformIO extra_scripts hook: generate the header for the active env.
    config_path = resolve_config_path()
    generate(config_path, os.path.join("include", "config.h"))
else:
    # Standalone use: python scripts/config.py [--validate] <device-name|path>
    import argparse

    parser = argparse.ArgumentParser(description="Validate and/or generate the config header.")
    parser.add_argument("config", help="device name (devices/<name>.yml) or path to a config YAML")
    parser.add_argument("--validate", action="store_true", help="validate only, do not write the header")
    args = parser.parse_args(sys.argv[1:])
    config_path = resolve_config_path(args.config)
    generate(config_path, os.path.join("include", "config.h"), write_header=not args.validate)

# Pydantic configuration schema for esp32-weather-epd.
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

from enum import Enum
import re
from typing import Optional
from typing import Annotated
from typing import Union, Literal
from pydantic import BaseModel, ConfigDict, Field, WithJsonSchema, model_validator


class DocEnum(Enum):
    def __new__(cls, value, doc=None):
        self = object.__new__(cls)  # calling super().__new__(value) here would fail
        self._value_ = value
        if doc is not None:
            self.__doc__ = doc
        return self


# ENUMS
class EpdPanel(DocEnum):
    """E-Paper panel type"""

    GENERIC_BW_V2 = "GENERIC_BW_V2", "7.5in e-Paper (v2) 800x480px Black/White"
    GENERIC_3C_B = "GENERIC_3C_B", "7.5in e-Paper (B) 800x480px Red/Black/White"
    DKE_3C_86BF = (
        "DKE_3C_86BF",
        "7.5in e-Paper (B) 800x480px Red/Black/White DEPG0750RWF86BF",
    )
    GENERIC_7C_F = "GENERIC_7C_F", "7.3in ACeP e-Paper (F) 800x480px 7-Colors"
    GENERIC_BW_V1 = "GENERIC_BW_V1", "7.5in e-Paper (v1) 640x384px Black/White"


class EpdDriver(str, Enum):
    """E-Paper driver board"""

    DESPI_C02 = "Good Display DESPI-C02"
    WAVESHARE = "Waveshare"


# Transfer Protocol
# HTTP
#   HTTP does not provide encryption or any security measures, making it highly
#   vulnerable to eavesdropping and data tampering. Has the advantage of using
#   less power.
# HTTPS_NO_CERT_VERIF
#   HTTPS without X.509 certificate verification provides encryption but lacks
#   authentication and is susceptible to man-in-the-middle attacks.
# HTTPS_WITH_CERT_VERIF
#   HTTPS with X.509 certificate verification offers the highest level of
#   security by providing encryption and verifying the identity of the server.
#
#   HTTPS with X.509 certificate verification comes with the draw back that
#   eventually the certificates on the esp32 will expire, requiring you to
#   update the certificates in cert.h and reflash this software.
#   Running cert.py will generate an updated cert.h file.
#   The current certificate for api.openweathermap.org is valid until
#   2026-04-10 23:59:59+00:00
class Transport(str, Enum):
    """Transport protocol for API requests"""

    HTTP = "HTTP"
    HTTPS_NO_VERIFY = "HTTPS_NO_VERIFY"
    HTTPS_VERIFY = "HTTPS_VERIFY"


class UnitsTemp(str, Enum):
    """Temperature units"""

    KELVIN = "Kelvin"
    CELSIUS = "Celsius"
    FAHRENHEIT = "Fahrenheit"


class UnitsSpeed(str, Enum):
    """Wind speed units"""

    METERSPERSECOND = "m/s"
    FEETPERSECOND = "ft/s"
    KILOMETERSPERHOUR = "km/h"
    MILESPERHOUR = "mph"
    KNOTS = "kt"
    BEAUFORT = "Beaufort"


class UnitsPres(str, Enum):
    """Atmospheric pressure units"""

    HECTOPASCALS = "hPa"
    PASCALS = "Pa"
    MILLIMETERSOFMERCURY = "mmHg"
    INCHESOFMERCURY = "inHg"
    MILLIBARS = "mbar"
    ATMOSPHERES = "atm"
    GRAMSPERSQUARECENTIMETER = "gsc"
    POUNDSPERSQUAREINCH = "psi"


class UnitsDistance(str, Enum):
    """Distance units"""

    KILOMETERS = "km"
    MILES = "mile"


class UnitsPrecip(str, Enum):
    """Precipitation units"""

    POP = "probability of precipitation"
    MILLIMETERS = "mm"
    CENTIMETERS = "cm"
    INCHES = "in"


class WindDirectionIndicator(str, Enum):
    NONE = "none"
    ARROW = "arrow"
    NUMBER = "number"
    CARDINAL = "cardinal"
    INTERCARDINAL = "intercardinal"
    SECONDARY_INTERCARDINAL = "secondary intercardinal"
    TERTIARY_INTERCARDINAL = "tertiary intercardinal"


class WindArrowPrecision(str, Enum):
    WIND_HIDDEN = "hidden"
    CARDINAL = "cardinal"
    INTERCARDINAL = "intercardinal"
    SECONDARY_INTERCARDINAL = "secondary intercardinal"
    TERTIARY_INTERCARDINAL = "tertiary intercardinal"
    ANY_360 = "360 deg"


class DisplayDailyPrecip(str, Enum):
    DISABLED = "disabled"
    ENABLED = "enabled"
    SMART = "smart"


class LogLevel(str, Enum):
    """Log verbosity threshold; messages below this level are suppressed"""

    TRACE = "trace"
    DEBUG = "debug"
    INFO = "info"
    WARNING = "warning"
    ERROR = "error"
    CRITICAL = "critical"


class Locale(str, Enum):
    DE_DE = "de_DE"
    EN_GB = "en_GB"
    EN_US = "en_US"
    ET_EE = "et_EE"
    ES_ES = "es_ES"
    FI_FI = "fi_FI"
    FR_FR = "fr_FR"
    IT_IT = "it_IT"
    NL_BE = "nl_BE"
    PT_BR = "pt_BR"


class Font(str, Enum):
    FREEMONO = "FreeMono"
    FREESANS = "FreeSans"
    FREESERIF = "FreeSerif"
    LATO = "Lato"
    NONTSERRAT = "Montserrat"
    OPEN_SANS = "Open Sans"
    POPPINS = "Poppins"
    QUICKSAND = "Quicksand"
    RALEWAY = "Raleway"
    ROBOTO = "Roboto"
    ROBOTO_MONO = "Roboto Mono"
    ROBOTO_SLAB = "Roboto Slab"
    UBUNTU = "Ubuntu"
    UBUNTU_MONO = "Ubuntu Mono"


class MoonPhaseStyle(str, Enum):
    PRIMARY = "primary"
    ALTERNATIVE = "alternative"


class MeteoAlarmCountry(DocEnum):
    """Country slug of the MeteoAlarm Atom feed
    (https://feeds.meteoalarm.org/)"""

    ANDORRA = "andorra", "Andorra"
    AUSTRIA = "austria", "Austria"
    BELGIUM = "belgium", "Belgium"
    BOSNIA_HERZEGOVINA = "bosnia-herzegovina", "Bosnia and Herzegovina"
    BULGARIA = "bulgaria", "Bulgaria"
    CROATIA = "croatia", "Croatia"
    CYPRUS = "cyprus", "Cyprus"
    CZECHIA = "czechia", "Czech Republic"
    DENMARK = "denmark", "Denmark"
    ESTONIA = "estonia", "Estonia"
    FINLAND = "finland", "Finland"
    FRANCE = "france", "France"
    GERMANY = "germany", "Germany"
    GREECE = "greece", "Greece"
    HUNGARY = "hungary", "Hungary"
    ICELAND = "iceland", "Iceland"
    IRELAND = "ireland", "Ireland"
    ISRAEL = "israel", "Israel"
    ITALY = "italy", "Italy"
    LATVIA = "latvia", "Latvia"
    LITHUANIA = "lithuania", "Lithuania"
    LUXEMBOURG = "luxembourg", "Luxembourg"
    MALTA = "malta", "Malta"
    MOLDOVA = "moldova", "Moldova"
    MONTENEGRO = "montenegro", "Montenegro"
    NETHERLANDS = "netherlands", "Netherlands"
    NORTH_MACEDONIA = "republic-of-north-macedonia", "Republic of North Macedonia"
    NORWAY = "norway", "Norway"
    POLAND = "poland", "Poland"
    PORTUGAL = "portugal", "Portugal"
    ROMANIA = "romania", "Romania"
    SERBIA = "serbia", "Serbia"
    SLOVAKIA = "slovakia", "Slovakia"
    SLOVENIA = "slovenia", "Slovenia"
    SPAIN = "spain", "Spain"
    SWEDEN = "sweden", "Sweden"
    SWITZERLAND = "switzerland", "Switzerland"
    UKRAINE = "ukraine", "Ukraine"
    UNITED_KINGDOM = "united-kingdom", "United Kingdom (GB/NI)"


# END ENUMS


class OpenMeteoForecastConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")
    provider: Literal["open_meteo_forecast"] = "open_meteo_forecast"
    transport: Transport = Transport.HTTPS_VERIFY


class NoaaForecastConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")
    provider: Literal["noaa_forecast"] = "noaa_forecast"


class MeteoSwissForecastConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")
    provider: Literal["meteoswiss_forecast"] = "meteoswiss_forecast"
    # MeteoSwiss local forecast point_id (postal-code centers use point_type_id=2).
    forecastPointId: str = Field(pattern=r"^\d{6}$")
    # SwissMetNet station abbreviation used for measured current conditions.
    stationId: str = Field(pattern=r"^[A-Z0-9]+$")


class OpenMeteoAirQualityConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")
    provider: Literal["open_meteo_air_quality"] = "open_meteo_air_quality"
    transport: Transport = Transport.HTTPS_VERIFY


class OpenWeatherMapOneCallV3Config(BaseModel):
    model_config = ConfigDict(extra="forbid")
    provider: Literal["openweathermap_onecall_v3"] = "openweathermap_onecall_v3"
    transport: Transport = Transport.HTTPS_VERIFY
    apiKey: str


class OpenWeatherMapOneCallV4Config(BaseModel):
    model_config = ConfigDict(extra="forbid")
    provider: Literal["openweathermap_onecall_v4"] = "openweathermap_onecall_v4"
    transport: Transport = Transport.HTTPS_VERIFY
    apiKey: str
    alerts: bool = False


class OpenWeatherMapAirQualityConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")
    provider: Literal["openweathermap_air_quality"] = "openweathermap_air_quality"
    transport: Transport = Transport.HTTPS_VERIFY
    apiKey: str


class MeteoAlarmAlertConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")
    provider: Literal["meteoalarm_alert"] = "meteoalarm_alert"
    country: MeteoAlarmCountry


class BME280PinsConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    power: int = 27
    sda: int = 21
    scl: int = 22


class BME280ProviderConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")
    provider: Literal["bme280"] = "bme280"
    pins: BME280PinsConfig = Field(default_factory=BME280PinsConfig)
    address: int = 0x76


ProviderConfig = Annotated[
    Union[
        OpenMeteoForecastConfig,
        NoaaForecastConfig,
        MeteoSwissForecastConfig,
        OpenMeteoAirQualityConfig,
        OpenWeatherMapOneCallV3Config,
        OpenWeatherMapOneCallV4Config,
        OpenWeatherMapAirQualityConfig,
        MeteoAlarmAlertConfig,
        BME280ProviderConfig,
    ],
    Field(discriminator="provider"),
]


defined_enums: list[Enum] = [
    EpdPanel,
    EpdDriver,
    UnitsTemp,
    UnitsSpeed,
    UnitsPres,
    UnitsDistance,
    UnitsPrecip,
    WindDirectionIndicator,
    WindArrowPrecision,
    DisplayDailyPrecip,
]



def enum_schema(enum: Enum):
    return WithJsonSchema(
        {
            "anyOf": [
                {"const": entry.value, "description": entry.__doc__} for entry in enum
            ],
            "description": enum.__doc__,
        }
    )


class StaticIpConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    ip: str
    gateway: str
    subnet: str
    dns1: Optional[str] = None
    dns2: Optional[str] = None

    @model_validator(mode="after")
    def validate_ip_addresses(self):
        ip_pattern = r"^(\d{1,3}\.){3}\d{1,3}$"

        # Check dns2 requires dns1
        if self.dns2 is not None and self.dns1 is None:
            raise ValueError("dns2 can only be provided if dns1 is also provided")

        fields_to_validate = {
            "ip": self.ip,
            "gateway": self.gateway,
            "subnet": self.subnet,
        }

        if self.dns1 is not None:
            fields_to_validate["dns1"] = self.dns1
        if self.dns2 is not None:
            fields_to_validate["dns2"] = self.dns2

        for field_name, field_value in fields_to_validate.items():
            if not re.match(ip_pattern, field_value):
                raise ValueError(
                    f"Invalid {field_name} format: '{field_value}'. "
                    "Expected format: XXX.XXX.XXX.XXX"
                )

        return self


class Wifi(BaseModel):
    model_config = ConfigDict(extra="forbid")

    ssid: str
    password: str
    timeoutMs: int = 10000
    scan: bool = False
    bssid: Optional[str] = None
    staticIp: Optional[StaticIpConfig] = None

    @model_validator(mode="after")
    def validate_scan_and_bssid(self):
        if self.scan and self.bssid is not None:
            raise ValueError(
                "wifi.scan and wifi.bssid cannot be enabled simultaneously. "
                "Either use scan to find the best network or specify a BSSID."
            )
        return self

    @model_validator(mode="after")
    def validate_bssid_format(self):
        if self.bssid is not None:
            # Match MAC address in format XX:XX:XX:XX:XX:XX only
            mac_pattern = r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$"
            if not re.match(mac_pattern, self.bssid):
                raise ValueError(
                    f"Invalid BSSID format: '{self.bssid}'. "
                    "Expected format: XX:XX:XX:XX:XX:XX "
                    "(where X is a hexadecimal digit)"
                )
        return self

    def bssid_to_define_value(self):
        # Convert BSSID string to C++ uint8_t array format
        cleaned = self.bssid.replace(":", "").upper()
        hex_pairs = [cleaned[i : i + 2] for i in range(0, len(cleaned), 2)]
        formatted = ", ".join([f"0x{pair}" for pair in hex_pairs])
        return f"{{{formatted}}}"


class DisplayPinsConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    busy: int = 14
    chipSelect: int = 13
    reset: int = 21
    dataCommand: int = 22
    clock: int = 18
    miso: int = 19
    mosi: int = 23
    power: int = 26


class DisplayHardwareConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    panel: Annotated[EpdPanel, enum_schema(EpdPanel)] = EpdPanel.GENERIC_BW_V2
    driverBoard: EpdDriver = EpdDriver.DESPI_C02
    pins: DisplayPinsConfig = Field(default_factory=DisplayPinsConfig)


class HomeAssistantMqttConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    enabled: bool = False
    server: str = ""
    port: int = 1883
    username: str = ""
    password: str = ""
    deviceName: str = "Weather EPD"
    discoveryPrefix: str = "homeassistant"


class Color(str, Enum):
    BLACK = "black"
    RED = "red"

    def to_define_value(self):
        if self == Color.BLACK:
            return "GxEPD_BLACK"
        return "GxEPD_RED"


class NTPConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    # The first server is primary; the second is the fallback used by the
    # generated configuration interface.
    servers: list[str] = Field(
        default_factory=lambda: ["pool.ntp.org", "time.nist.gov"],
        min_length=1,
        max_length=2,
    )
    syncIntervalWakeups: int = 6
    # Auto-correct the RTC slow-clock drift: the correction factor is learned
    # from the deviation measured between consecutive NTP synchronizations and
    # applied to the deep-sleep timer and the internal clock, so that the
    # device wakes up (and displays) the correct time between NTP syncs.
    rtcCorrection: bool = True
    # If you encounter the 'Failed To Fetch The Time' error, try increasing
    # NTP_TIMEOUT or select closer/lower latency time servers.
    timeoutMs: int = 20000


class Colors(BaseModel):
    model_config = ConfigDict(extra="forbid")

    outlookLowThresholdTemperature: int = 0
    outlookHighThresholdTemperature: int = 35
    outlookTemperatureLowColor: Color = Color.BLACK
    outlookTemperatureNormalColor: Color = Color.BLACK
    outlookTemperatureHighColor: Color = Color.BLACK
    outlookConditionsIconAccent: Color = Color.BLACK
    city: Color = Color.BLACK
    date: Color = Color.BLACK
    alert: Color = Color.BLACK
    errorIcon: Color = Color.BLACK
    statusBarBatteryWarning: Color = Color.BLACK
    statusBarWeakWifi: Color = Color.BLACK
    statusBarMessage: Color = Color.BLACK
    forecastPrecipitation: Color = Color.BLACK


class UnitsConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    temperature: UnitsTemp = UnitsTemp.CELSIUS
    speed: UnitsSpeed = UnitsSpeed.KILOMETERSPERHOUR
    pressure: UnitsPres = UnitsPres.MILLIBARS
    distance: UnitsDistance = UnitsDistance.KILOMETERS
    hourlyPrecipitation: UnitsPrecip = UnitsPrecip.POP
    dailyPrecipitation: UnitsPrecip = UnitsPrecip.MILLIMETERS


class StatusBarConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    showBatteryVoltage: bool = False
    showWifiRssi: bool = False


class RenderingConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    locale: Locale
    font: Font = Font.FREESANS
    units: UnitsConfig = Field(default_factory=UnitsConfig)
    windDirectionIndicator: WindDirectionIndicator = WindDirectionIndicator.ARROW
    windArrowPrecision: WindArrowPrecision = WindArrowPrecision.SECONDARY_INTERCARDINAL
    displayDailyPrecip: DisplayDailyPrecip = DisplayDailyPrecip.SMART
    displayHourlyIcons: bool = True
    moonPhaseStyle: MoonPhaseStyle = MoonPhaseStyle.PRIMARY
    hourlyGraphMax: int = 24
    timeFormat: str = "%H:%M"
    hourFormat: str = "%H"
    dateFormat: str
    refreshTimeFormat: str = "%x %H:%M"
    statusBar: StatusBarConfig = Field(default_factory=StatusBarConfig)
    leftPanelLayout: list[str] = Field(
        default_factory=lambda: [
            "SUNRISE",
            "SUNSET",
            "MOONRISE",
            "MOONSET",
            "MOONPHASE",
            "HUMIDITY",
            "WIND",
            "PRESSURE",
            "AIR_QUALITY",
            "VISIBILITY",
        ]
    )
    colors: Colors = Field(default_factory=Colors)


class LocationConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    latitude: float = Field(ge=-90, le=90)
    longitude: float = Field(ge=-180, le=180)
    city: str
    timezone: str


class BatteryConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    monitoring: bool = True
    adcPin: int = 35


class ScheduleConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    refreshMinutes: int = Field(default=30, ge=1)
    bedTime: int = Field(default=0, ge=0, le=23)
    wakeTime: int = Field(default=6, ge=0, le=23)


class ConfigSchema(BaseModel):
    model_config = ConfigDict(extra="forbid")

    display: DisplayHardwareConfig = Field(default_factory=DisplayHardwareConfig)
    providers: list[ProviderConfig]
    wifi: Wifi = Field(default_factory=Wifi)
    ntp: NTPConfig = Field(default_factory=NTPConfig)
    location: LocationConfig
    rendering: RenderingConfig
    battery: BatteryConfig = Field(default_factory=BatteryConfig)
    schedule: ScheduleConfig = Field(default_factory=ScheduleConfig)
    homeAssistantMqtt: HomeAssistantMqttConfig | None = None
    logLevel: LogLevel = LogLevel.INFO

    @model_validator(mode="before")
    @classmethod
    def validate_meteoswiss_station_presence(cls, values):
        if isinstance(values, dict):
            for provider in values.get("providers", []) or []:
                if (isinstance(provider, dict) and provider.get("provider") == "meteoswiss_forecast" and
                        "stationId" not in provider):
                    raise ValueError(
                        "meteoswiss_forecast requires stationId for current observations; "
                        "choose a SwissMetNet station from "
                        "https://www.meteoswiss.admin.ch/services-and-publications/applications/measurement-values.html"
                    )
        return values

    @model_validator(mode="after")
    def validate_left_panel_layout(self):
        allowed_left_panel_keys = {
            "SUNRISE",
            "SUNSET",
            "WIND",
            "HUMIDITY",
            "UVI",
            "PRESSURE",
            "INPRESSURE",
            "AIR_QUALITY",
            "VISIBILITY",
            "MOONRISE",
            "MOONSET",
            "MOONPHASE",
            "DEWPOINT",
        }
        invalid_keys = [
            k for k in self.rendering.leftPanelLayout if k not in allowed_left_panel_keys
        ]
        if invalid_keys:
            raise ValueError(
                f"Invalid keys in rendering.leftPanelLayout: {invalid_keys}. "
                f"Allowed keys are: {sorted(allowed_left_panel_keys)}"
            )
        duplicate_keys = [
            key for key in set(self.rendering.leftPanelLayout)
            if self.rendering.leftPanelLayout.count(key) > 1
        ]
        if duplicate_keys:
            raise ValueError(
                f"Duplicate entries in rendering.leftPanelLayout: {sorted(duplicate_keys)}"
            )
        if len(self.rendering.leftPanelLayout) > 10:
            raise ValueError("rendering.leftPanelLayout cannot contain more than 10 entries")
        return self

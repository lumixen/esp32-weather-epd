from enum import Enum
import re
from typing import Dict, Optional
from typing import Annotated
from pydantic import BaseModel, Field, WithJsonSchema, model_validator


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
class ApiProtocol(str, Enum):
    """API protocol for API requests"""

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


class WeatherAPI(str, Enum):
    OPEN_WEATHER_MAP = "OpenWeatherMap"
    OPEN_METEO = "Open-Meteo"


class AirQualityAPI(str, Enum):
    OPEN_WEATHER_MAP = "OpenWeatherMap"
    OPEN_METEO = "Open-Meteo"


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


# END ENUMS

defined_enums: list[Enum] = [
    EpdPanel,
    EpdDriver,
    WeatherAPI,
    AirQualityAPI,
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
    ssid: str
    password: str
    timeout: int = 10000
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
    
    def bssid_to_config_value(self):
        # Convert BSSID string to C++ uint8_t array format
        cleaned = self.bssid.replace(":", "").upper()
        hex_pairs = [cleaned[i : i + 2] for i in range(0, len(cleaned), 2)]
        formatted = ", ".join([f"0x{pair}" for pair in hex_pairs])
        return f"{{{formatted}}}"


class PinsConfig(BaseModel):
    batAdc: int = 35
    epdBusy: int = 14
    epdCS: int = 13
    epdRst: int = 21
    epdDC: int = 22
    epdSCK: int = 18
    epdMISO: int = 19
    epdMOSI: int = 23
    epdPwr: int = 26


class HomeAssistantMqttConfig(BaseModel):
    enabled: bool = False
    server: str = ""
    port: int = 1883
    username: str = ""
    password: str = ""
    clientId: str = "esp32-weather-epd"
    deviceName: str = "Weather EPD"
    discoveryPrefix: str = "homeassistant"


class Color(str, Enum):
    BLACK = "black"
    RED = "red"

    def to_config_value(self):
        if self == Color.BLACK:
            return "GxEPD_BLACK"
        return "GxEPD_RED"


class Colors(BaseModel):
    outlookThresholdTemperature: int = 0
    outlookTemperatureBelowThreshold: Color = Color.BLACK
    outlookTemperatureAboveThreshold: Color = Color.BLACK
    outlookConditionsIconAccent: Color = Color.BLACK
    city: Color = Color.BLACK
    date: Color = Color.BLACK
    alert: Color = Color.BLACK
    errorIcon: Color = Color.BLACK
    statusBarBatteryWarning: Color = Color.BLACK
    statusBarWeakWifi: Color = Color.BLACK
    statusBarMessage: Color = Color.BLACK


class ConfigSchema(BaseModel):
    epdPanel: Annotated[EpdPanel, enum_schema(EpdPanel)] = EpdPanel.GENERIC_BW_V2
    epdDriver: EpdDriver = EpdDriver.DESPI_C02
    locale: Locale
    apiProtocol: ApiProtocol = ApiProtocol.HTTPS_VERIFY
    weatherAPI: WeatherAPI = WeatherAPI.OPEN_METEO
    airQualityAPI: AirQualityAPI = AirQualityAPI.OPEN_METEO
    ntpSyncIntervalHours: int = 6
    useImperialUnitsAsDefault: bool = False
    unitsTemp: UnitsTemp = Field(
        default_factory=lambda data: UnitsTemp.FAHRENHEIT
        if data["useImperialUnitsAsDefault"]
        else UnitsTemp.CELSIUS
    )
    unitsSpeed: UnitsSpeed = Field(
        default_factory=lambda data: UnitsSpeed.MILESPERHOUR
        if data["useImperialUnitsAsDefault"]
        else UnitsSpeed.KILOMETERSPERHOUR
    )
    unitsPres: UnitsPres = Field(
        default_factory=lambda data: UnitsPres.INCHESOFMERCURY
        if data["useImperialUnitsAsDefault"]
        else UnitsPres.MILLIBARS
    )
    unitsDistance: UnitsDistance = Field(
        default_factory=lambda data: UnitsDistance.MILES
        if data["useImperialUnitsAsDefault"]
        else UnitsDistance.KILOMETERS
    )
    unitsHourlyPrecip: UnitsPrecip = UnitsPrecip.POP
    unitsDailyPrecip: UnitsPrecip = Field(
        default_factory=lambda data: UnitsPrecip.INCHES
        if data["useImperialUnitsAsDefault"]
        else UnitsPrecip.MILLIMETERS
    )
    windDirectionIndicator: WindDirectionIndicator = WindDirectionIndicator.ARROW
    windArrowPrecision: WindArrowPrecision = WindArrowPrecision.SECONDARY_INTERCARDINAL
    font: Font = Font.FREESANS
    displayDailyPrecip: DisplayDailyPrecip = DisplayDailyPrecip.SMART
    displayHourlyIcons: bool = True
    displayAlerts: bool = True
    statusBarExtrasBatVoltage: bool = False
    statusBarExtrasWifiRSSI: bool = False
    batteryMonitoring: bool = True
    debugLevel: int = 0  # TODO: From 0 to 2
    pin: PinsConfig = Field(default_factory=PinsConfig)
    wifi: Wifi = Field(default_factory=Wifi)
    owmApikey: str | None = None
    owmOnecallVersion: str = "3.0"
    latitude: str
    longitude: str
    city: str
    timezone: str
    timeFormat: str = "%H:%M"
    hourFormat: str = "%H"
    dateFormat: str
    refreshTimeFormat: str = "%x %H:%M"
    sleepDuration: int = 30
    bedTime: int = 0
    wakeTime: int = 6
    hourlyGraphMax: int = 24
    homeAssistantMqtt: HomeAssistantMqttConfig | None = None
    leftPanelLayout: Dict[str, int] = Field(
        default_factory=lambda: {
            "SUNRISE": 0,
            "SUNSET": 1,
            "MOONRISE": 2,
            "MOONSET": 3,
            "MOONPHASE": 4,
            "HUMIDITY": 5,
            "WIND": 6,
            "PRESSURE": 7,
            "AIR_QUALITY": 8,
            "VISIBILITY": 9,
        }
    )
    moonPhaseStyle: MoonPhaseStyle = MoonPhaseStyle.PRIMARY
    colors: Colors = Field(default_factory=Colors)

    @model_validator(mode="after")
    def validate_apikey(self):
        if (
            self.weatherAPI == WeatherAPI.OPEN_WEATHER_MAP
            or self.airQualityAPI == AirQualityAPI.OPEN_WEATHER_MAP
        ) and not self.owmApikey:
            raise ValueError("The API key is required on OpenWeatherMap")
        return self

    @model_validator(mode="after")
    def validate_left_panel_layout(self):
        allowed_left_panel_keys = {
            "SUNRISE",
            "SUNSET",
            "WIND",
            "HUMIDITY",
            "UVI",
            "PRESSURE",
            "AIR_QUALITY",
            "VISIBILITY",
            "MOONRISE",
            "MOONSET",
            "MOONPHASE",
            "DEWPOINT",
        }
        invalid_keys = [
            k for k in self.leftPanelLayout.keys() if k not in allowed_left_panel_keys
        ]
        if invalid_keys:
            raise ValueError(
                f"Invalid keys in leftPanelLayout: {invalid_keys}. "
                f"Allowed keys are: {sorted(allowed_left_panel_keys)}"
            )
        invalid_indices = [
            (k, v)
            for k, v in self.leftPanelLayout.items()
            if not isinstance(v, int) or v < 0 or v > 9
        ]
        if invalid_indices:
            raise ValueError(
                f"Invalid indices in leftPanelLayout (must be integer between 0 and 9): {invalid_indices}"
            )
        return self

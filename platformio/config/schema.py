from enum import Enum
from typing import Dict
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


class ConfigSchema(BaseModel):
    epdPanel: Annotated[EpdPanel, enum_schema(EpdPanel)] = EpdPanel.GENERIC_BW_V2
    epdDriver: EpdDriver = EpdDriver.DESPI_C02
    locale: Locale
    weatherAPI: WeatherAPI = WeatherAPI.OPEN_METEO
    airQualityAPI: AirQualityAPI = AirQualityAPI.OPEN_METEO
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
    wifiSSID: str
    wifiPassword: str
    wifiTimeout: int = 10000
    wifiScan: bool = False
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
    leftPanelPositions: Dict[str, int] = {}
    moonPhaseStyle: MoonPhaseStyle = MoonPhaseStyle.PRIMARY

    @model_validator(mode="after")
    def validate_apikey(self):
        if (
            self.weatherAPI == WeatherAPI.OPEN_WEATHER_MAP
            or self.airQualityAPI == AirQualityAPI.OPEN_WEATHER_MAP
        ) and not self.owmApikey:
            raise ValueError("The API key is required on OpenWeatherMap")
        return self

    @model_validator(mode="after")
    def validate_left_panel_positions(self):
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
            k
            for k in self.leftPanelPositions.keys()
            if k not in allowed_left_panel_keys
        ]
        if invalid_keys:
            raise ValueError(
                f"Invalid keys in leftPanelPositions: {invalid_keys}. "
                f"Allowed keys are: {sorted(allowed_left_panel_keys)}"
            )
        invalid_indices = [
            (k, v)
            for k, v in self.leftPanelPositions.items()
            if not isinstance(v, int) or v < 0 or v > 9
        ]
        if invalid_indices:
            raise ValueError(
                f"Invalid indices in leftPanelPositions (must be integer between 0 and 9): {invalid_indices}"
            )
        return self

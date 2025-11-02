from enum import Enum
from typing import Annotated
from pydantic import BaseModel, Field, WithJsonSchema, field_validator, model_validator

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
    DISP_BW_V2 = "DISP_BW_V2", "7.5in e-Paper (v2) 800x480px Black/White"
    DISP_3C_B = "DISP_3C_B", "7.5in e-Paper (B) 800x480px Red/Black/White"
    DISP_3C_86BF = "DISP_3C_86BF", "7.5in e-Paper (B) 800x480px Red/Black/White DEPG0750RWF86BF"
    DISP_7C_F = "DISP_7C_F", "7.3in ACeP e-Paper (F) 800x480px 7-Colors"
    DISP_BW_V1 = "DISP_BW_V1", "7.5in e-Paper (v1) 640x384px Black/White"

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
    HECTOPASCAL = "hPa"
    PASCAL = "Pa"
    MILLIMETERSOFMERCURY = "mmHg"
    INCHESOFMERCURY = "inHg"
    MILLIBAR = "mbar"
    ATMOSPHERE = "atm"
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

class WindDirectionLabel(str, Enum):
    WIND_HIDDEN = "hidden"
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
    PRECIP_DISABLED = "disabled"
    PRECIP_ENABLED = "enabled"
    PRECIP_SMART = "smart"

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
    WindDirectionLabel,
    WindArrowPrecision,
    DisplayDailyPrecip
]

def enum_schema(enum: Enum):
    return WithJsonSchema({
            'anyOf': [
                {
                    "const": entry.value,
                    "description": entry.__doc__
                } for entry in enum
            ],
            "description": enum.__doc__
        })

class ConfigSchema(BaseModel):
    epdPanel: Annotated[EpdPanel, enum_schema(EpdPanel)] = EpdPanel.DISP_BW_V2
    epdDriver: EpdDriver = EpdDriver.DESPI_C02
    locale: Locale
    weatherAPI: WeatherAPI = WeatherAPI.OPEN_WEATHER_MAP
    airQualityAPI: AirQualityAPI = AirQualityAPI.OPEN_WEATHER_MAP
    useImperialUnitsAsDefault: bool = False # TODO: Use locale to set units
    unitsTemp: UnitsTemp = Field(default_factory=lambda data: UnitsTemp.FAHRENHEIT if data['useImperialUnitsAsDefault'] else UnitsTemp.CELSIUS)
    unitsSpeed: UnitsSpeed = Field(default_factory=lambda data: UnitsSpeed.MILESPERHOUR if data['useImperialUnitsAsDefault'] else UnitsSpeed.KILOMETERSPERHOUR)
    unitsPres: UnitsPres = Field(default_factory=lambda data: UnitsPres.INCHESOFMERCURY if data['useImperialUnitsAsDefault'] else UnitsPres.MILLIBAR)
    unitsDistance: UnitsDistance = Field(default_factory=lambda data: UnitsDistance.MILES if data['useImperialUnitsAsDefault'] else UnitsDistance.KILOMETERS)
    unitsHourlyPrecip: UnitsPrecip = UnitsPrecip.POP
    unitsDailyPrecip: UnitsPrecip = Field(default_factory=lambda data: UnitsPrecip.INCHES if data['useImperialUnitsAsDefault'] else UnitsPrecip.MILLIMETERS)
    windDirectionLabel: WindDirectionLabel = WindDirectionLabel.WIND_HIDDEN
    windArrowPrecision: WindArrowPrecision = WindArrowPrecision.SECONDARY_INTERCARDINAL
    font: Font = Font.FREESANS
    displayDailyPrecip: DisplayDailyPrecip = DisplayDailyPrecip.PRECIP_SMART
    displayHourlyIcons: bool = True
    displayAlerts: bool = True
    statusBarExtrasBatVoltage: bool = False
    statusBarExtrasWifiRSSI: bool = False
    batteryMonitoring: bool = True
    debugLevel: int = 0 # TODO: From 0 to 2
    pinBatAdc: int = 34 # TODO: Manage An nomenclature for analog pins
    pinEpdBusy: int = 14
    pinEpdCS: int = 13
    pinEpdRst: int = 21
    pinEpdDC: int = 22
    pinEpdSCK: int = 18
    pinEpdMISO: int = 19
    pinEpdMOSI: int = 23
    pinEpdPwr: int = 26
    wifiSSID: str
    wifiPassword: str
    owmApikey: str | None = None
    owmOnecallVersion: str = "3.0"
    latitude: str
    longitude: str
    city: str
    timezone: str
    # TODO: Use locale to set formats
    timeFormat: str = "%H:%M"
    hourFormat: str = "%H"
    dateFormat: str
    refreshTimeFormat: str = "%x %H:%M"
    sleepDuration: int = 30
    bedTime: int = 0
    wakeTime: int = 6
    hourlyGraphMax: int = 24
    
    @model_validator(mode="after")
    def validate_apikey(self):
        if self.owmApikey is None and self.weatherAPI == WeatherAPI.OPEN_WEATHER_MAP:
            raise ValueError("The API key is required on OpenWeatherMap")
        return self

# TODO: JSON Schema
#main_model_schema = ConfigSchema.model_json_schema()
#print(json.dumps(main_model_schema, indent=4))
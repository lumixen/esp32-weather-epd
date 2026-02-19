## Highlights

- Optimized for the Lolin D32 board.
- Uses [Open-Meteo](https://open-meteo.com/) as the primary weather API.
- Configuration is managed via a non-versioned YAML file.
- Supports the DKE DEPG0750RWF86BF e-paper display.
- Home Assistant integration through MQTT.

### ESP32 E-Paper Weather Display

<p float="left">
  <img src="https://github.com/user-attachments/assets/15d49106-3b07-4fbe-b252-1a642cb1251c" width="49%" />
  <img src="https://github.com/user-attachments/assets/d7e9c4d2-8885-43cd-aa22-df5e04820dd2" width="49%" />
</p>

Enclosure files and assembly instructions are available at [printables](https://www.printables.com/model/1469770-75-e-paper-frame-for-lolin-d32-waveshare-driver).

### Panel Support

  | Panel                                   | Resolution | Colors          | Notes                                                                                                                 |
  |-----------------------------------------|------------|-----------------|-----------------------------------------------------------------------------------------------------------------------|
  | DKE DEPG0750RWF86BF 7.5in e-paper (86BF)  | 800x480px  | Red/Black/White     | Available [here](https://www.aliexpress.com/item/1005006254055758.html)                 |
  | Waveshare 7.5in e-paper (v2)            | 800x480px  | Black/White     | Available [here](https://www.waveshare.com/product/7.5inch-e-paper.htm)                                |
  | Good Display 7.5in e-paper (GDEY075T7)  | 800x480px  | Black/White     | Available [here](https://www.aliexpress.com/item/3256802683908868.html)             |
  | Waveshare 7.5in e-Paper (B)             | 800x480px  | Red/Black/White | Available [here](https://www.waveshare.com/product/7.5inch-e-paper-b.htm).                                            |
  | Good Display 7.5in e-paper (GDEY075Z08) | 800x480px  | Red/Black/White | Available [here](https://www.aliexpress.com/item/3256803540460035.html).                                              |
  | Waveshare 7.3in ACeP e-Paper (F)        | 800x480px  | 7-Color         | Available [here](https://www.waveshare.com/product/displays/e-paper/epaper-1/7.3inch-e-paper-f.htm).                  |
  | Good Display 7.3in e-paper (GDEY073D46) | 800x480px  | 7-Color         | Available [here](https://www.aliexpress.com/item/3256805485098421.html).                                              |
  | Waveshare 7.5in e-paper (v1)            | 640x384px  | Black/White     | Limited support. Some information not displayed.                 |
  | Good Display 7.5in e-paper (GDEW075T8)  | 640x384px  | Black/White     | Limited support. Some information not displayed.                 |


### Setup Guide

1. **Connect the Hardware**
   - Wire your Lolin D32 board to the e-paper HAT driver according to the wiring schematic provided below.

2. **Install Dependencies**
   - Make sure you have [PlatformIO](https://platformio.org/) installed in VS Code.

3. **Configure the Software**
   - Create a `config.yml` file in the `platformio` folder.
   - Copy and edit the example configuration to match your hardware and preferences (WiFi credentials, location, panel type, etc.).

4. **Compile and Upload**
   - Open the project in VS Code.
   - Click the PlatformIO "Build" button to compile the firmware.
   - Connect your ESP32 board via USB and click "Upload" to flash the firmware.

### Wiring Schematic (Lolin D32 Board)

Wiring is specific for Lolin D32 board:

<img width="459" height="525" alt="image" src="https://github.com/user-attachments/assets/278b804c-fa89-4595-b60a-8fa0e6571931" />

| E-Paper Pin    | Lolin D32 Pin |
|----------------|--------------|
| PWR            | GPIO2        |
| BUSY           | GPIO4        |
| RST            | GPIO16       |
| DC             | GPIO17       |
| CS             | GPIO5        |
| CLK            | GPIO18       |
| DIN            | GPIO23       |
| VCC            | 3v3          |


### Configuration

To configure the build, create a new `config.yml` file in the `platformio` folder with the configuration variables. For example:
```yaml
epdPanel: DKE_3C_86BF
epdDriver: Waveshare
locale: en_US
weatherAPI: Open-Meteo
airQualityAPI: Open-Meteo
pin:
  batAdc: 35
  epdBusy: 4
  epdCS: 5
  epdRst: 16
  epdDC: 17
  epdSCK: 18
  epdMISO: 19
  epdMOSI: 23
  epdPwr: 2
useImperialUnitsAsDefault: false
unitsTemp: Celsius
unitsSpeed: km/h
unitsPres: mbar
unitsDistance: km
unitsHourlyPrecip: probability of precipitation
unitsDailyPrecip: mm
windDirectionIndicator: arrow
windArrowPrecision: secondary intercardinal
font: FreeSans
displayDailyPrecip: smart
displayHourlyIcons: true
displayAlerts: false
batteryMonitoring: true
statusBarExtrasBatVoltage: true
statusBarExtrasWifiRSSI: false
debugLevel: 0
wifi:
  ssid: SSID
  password: PASSWORD
  # Optional
  # timeout: 10000
  # scan: false
  # bssid: "XX:XX:XX:XX:XX:XX"
  # staticIp:
  #   ip: XXX.XXX.XXX.XXX
  #   gateway: XXX.XXX.XXX.XXX
  #   subnet: XXX.XXX.XXX.XXX
  #   dns1: XXX.XXX.XXX.XXX
owmApikey:
owmOnecallVersion: ""
latitude: "64"
longitude: "-22"
city: ESPLand
timezone: UTC0
timeFormat: "%H:%M"
hourFormat: "%H"
dateFormat: "%d/%m/%Y"
refreshTimeFormat: "%x %H:%M"
sleepDuration: 30
bedTime: 0
wakeTime: 6
hourlyGraphMax: 24
moonPhaseStyle: alternative
leftPanelPositions:
  SUNRISE: 0
  SUNSET: 1
  MOONRISE: 2
  MOONSET: 3
  MOONPHASE: 4
  HUMIDITY: 5
  WIND: 6
  PRESSURE: 7
  AIR_QUALITY: 8
  VISIBILITY: 9
homeAssistantMqtt:
  enabled: true
  server: XXX.XXX.XXX.XXX
  port: 1883
  username: USERNAME
  password: PASSWORD
  clientId: esp32-weather-epd
  deviceName: Device Name
  discoveryPrefix: homeassistant
colors:
  outlookTemperatureBelowFreezing: red
  outlookTemperatureAboveFreezing: black
  outlookConditionsIconAccent: red
  city: black
  date: red
  alert: red
  errorIcon: red
  statusBarBatteryWarning: red
  statusBarWeakWifi: red
  statusBarMessage: red
```

### Configuration Options

| Option                    | Description                                                                 | Default Value                          |
|---------------------------|-----------------------------------------------------------------------------|----------------------------------------|
| epdPanel                  | E-Paper panel type                                                          | GENERIC_BW_V2                          |
| epdDriver                 | E-Paper driver board                                                        | DESPI_C02                              |
| locale                    | Locale/language                                                             | (required)                             |
| apiProtocol               | API protocol for API requests                                               | HTTPS_VERIFY                           |
| weatherAPI                | Weather API provider                                                        | Open-Meteo                             |
| airQualityAPI             | Air Quality API provider                                                    | Open-Meteo                             |
| ntpSyncIntervalHours      | NTP sync interval in hours                                                  | 6                                      |
| useImperialUnitsAsDefault | Sets all units to impertial when true                                       | false                                  |
| unitsTemp                 | Temperature units                                                           | Celsius or Fahrenheit                  |
| unitsSpeed                | Wind speed units                                                            | km/h or mph                            |
| unitsPres                 | Atmospheric pressure units                                                  | mbar or inHg                           |
| unitsDistance             | Distance units                                                              | km or miles                            |
| unitsHourlyPrecip         | Hourly precipitation units                                                  | probability of precipitation           |
| unitsDailyPrecip          | Daily precipitation units                                                   | mm or inches                           |
| windDirectionIndicator    | Wind direction indicator style                                              | arrow                                  |
| windArrowPrecision        | Wind arrow precision                                                        | secondary intercardinal                |
| font                      | Font for display                                                            | FreeSans                               |
| displayDailyPrecip        | Display daily precipitation                                                 | smart                                  |
| displayHourlyIcons        | Display hourly weather icons                                                | true                                   |
| displayAlerts             | Display weather alerts                                                      | true                                   |
| statusBarExtrasBatVoltage | Show battery voltage in status bar                                          | false                                  |
| statusBarExtrasWifiRSSI   | Show WiFi RSSI in status bar                                                | false                                  |
| batteryMonitoring         | Enable battery monitoring                                                   | true                                   |
| debugLevel                | Debug level (0-2)                                                           | 0                                      |
| pin                       | Pin configuration (see below)                                               | See PinsConfig                         |
| wifi                      | WiFi configuration (see below)                                              | (required fields: ssid, password)      |
| owmApikey                 | OpenWeatherMap API key                                                      | None                                   |
| owmOnecallVersion         | OpenWeatherMap OneCall API version                                          | "3.0"                                  |
| latitude                  | Latitude                                                                    | (required)                             |
| longitude                 | Longitude                                                                   | (required)                             |
| city                      | City name                                                                   | (required)                             |
| timezone                  | Timezone                                                                    | (required)                             |
| timeFormat                | Time format string                                                          | "%H:%M"                                |
| hourFormat                | Hour format string                                                          | "%H"                                   |
| dateFormat                | Date format string                                                          | (required)                             |
| refreshTimeFormat         | Refresh time format string                                                  | "%x %H:%M"                             |
| sleepDuration             | Sleep duration in minutes                                                   | 30                                     |
| bedTime                   | Bed time hour (0-23)                                                        | 0                                      |
| wakeTime                  | Wake time hour (0-23)                                                       | 6                                      |
| hourlyGraphMax            | Max hours to show in hourly graph                                           | 24                                     |
| homeAssistantMqtt         | Home Assistant MQTT config (see below)                                      | None                                   |
| leftPanelLayout           | Left panel layout mapping (see below)                                       | See schema                             |
| moonPhaseStyle            | Moon phase icon style                                                       | primary                                |
| colors                    | Color configuration (see below)                                             | See Colors                             |

#### Pin Configuration (`pin`)
| Option    | Description           | Default |
|-----------|-----------------------|---------|
| batAdc    | Battery ADC pin       | 35      |
| epdBusy   | EPD busy pin          | 14      |
| epdCS     | EPD chip select pin   | 13      |
| epdRst    | EPD reset pin         | 21      |
| epdDC     | EPD data/command pin  | 22      |
| epdSCK    | EPD SPI clock pin     | 18      |
| epdMISO   | EPD SPI MISO pin      | 19      |
| epdMOSI   | EPD SPI MOSI pin      | 23      |
| epdPwr    | EPD power pin         | 26      |

#### WiFi Configuration (`wifi`)
| Option    | Description           | Default  |
|-----------|-----------------------|----------|
| ssid      | WiFi SSID             | (required) |
| password  | WiFi password         | (required) |
| timeout   | Connection timeout ms | 10000    |
| scan      | Enable WiFi scan      | false    |
| bssid     | Lock to BSSID         | None     |
| staticIp  | Static IP config      | None     |

#### Home Assistant MQTT (`homeAssistantMqtt`)
| Option          | Description                | Default                |
|-----------------|---------------------------|------------------------|
| enabled         | Enable MQTT integration    | false                  |
| server          | MQTT server address        |                        |
| port            | MQTT server port           | 1883                   |
| username        | MQTT username             |                        |
| password        | MQTT password             |                        |
| clientId        | MQTT client ID            | esp32-weather-epd      |
| deviceName      | Device name               | Weather EPD            |
| discoveryPrefix | Home Assistant prefix     | homeassistant          |

#### Colors (`colors`)
| Option                         | Description                                 | Default |
|---------------------------------|---------------------------------------------|---------|
| outlookTemperatureBelowFreezing | Color for below freezing temperature        | black   |
| outlookTemperatureAboveFreezing | Color for above freezing temperature        | black   |
| outlookConditionsIconAccent     | Accent color for condition icons            | black   |
| city                           | City text color                             | black   |
| date                           | Date text color                             | black   |
| alert                          | Alert color                                 | black   |
| errorIcon                      | Error icon color                            | black   |
| statusBarBatteryWarning         | Battery warning color                       | black   |
| statusBarWeakWifi               | Weak WiFi color                             | black   |
| statusBarMessage                | Status bar message color                    | black   |

#### Left Panel Layout (`leftPanelLayout`)
A mapping of panel items to their display order (0-9). Allowed keys: SUNRISE, SUNSET, MOONRISE, MOONSET, MOONPHASE, HUMIDITY, WIND, PRESSURE, AIR_QUALITY, VISIBILITY, UVI, DEWPOINT.

### Home Assistant integration through MQTT

The device supports Home Assistant integration via MQTT for monitoring. When enabled, the device publishes sensor data and device information using Home Assistant's MQTT discovery protocol.

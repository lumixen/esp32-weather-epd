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
   - Wire your Lolin D32 board to the e-paper panel according to the wiring schematic provided below.

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

| Signal         | Lolin D32 Pin | E-Paper Pin      | Description                |
|----------------|--------------|------------------|----------------------------|
| EPD Busy       | 4            | BUSY             | E-paper busy signal        |
| EPD Chip Select| 5            | CS               | SPI chip select            |
| EPD Reset      | 16           | RST              | E-paper reset              |
| EPD Data/Command| 17          | DC               | Data/command select        |
| EPD Clock      | 18           | SCK              | SPI clock                  |
| EPD MISO       | 19           | MISO             | SPI MISO                   |
| EPD MOSI       | 23           | MOSI             | SPI MOSI                   |
| EPD PWR        | 2            | VCC/PWR          | E-paper power              |


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
wifiSSID: SSID
wifiPassword: PASSWORD
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
homeAssistantMqtt:
  enabled: true
  server: XXX.XXX.XXX.XXX
  port: 1883
  username: USERNAME
  password: PASSWORD
  clientId: esp32-weather-epd
  deviceName: Device Name
  discoveryPrefix: homeassistant
```

### Home Assistant integration through MQTT

The device supports Home Assistant integration via MQTT for monitoring. When enabled, the device publishes sensor data and device information using Home Assistant's MQTT discovery protocol.

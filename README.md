## Changes made to this fork
- Open-Meteo is the main weather source. No need to provide the credit card details to get the OWM token.
- Configuration moved to a non-versioned JSON file which is processed by a Python script to generate C++ defines.
- Added support for DKE DEPG0750RWF86BF display.
- Removed BME integration.
- Adjusted specifically for Lolin D32 board.

---

# ESP32 E-Paper Weather Display
<!-- 
<p float="left">
  <img src="showcase/assembled-demo-raleigh-front.jpg" />
  <img src="showcase/assembled-demo-raleigh-side.jpg" width="49%" />
  <img src="showcase/assembled-demo-raleigh-back.jpg" width="49%" />
  <img src="showcase/assembled-demo-bottom-cover.jpg" width="49%" />
  <img src="showcase/assembled-demo-bottom-cover-removed.jpg" width="49%" />
</p> -->


### Panel Support

  | Panel                                   | Resolution | Colors          | Notes                                                                                                                 |
  |-----------------------------------------|------------|-----------------|-----------------------------------------------------------------------------------------------------------------------|
  | DKE DEPG0750RWF86BF 7.5in e-paper (86BF)  | 800x480px  | Red/Black/White     | Available [here](https://www.aliexpress.com/item/1005006254055758.html)                 |
  | Waveshare 7.5in e-paper (v2)            | 800x480px  | Black/White     | Available [here](https://www.waveshare.com/product/7.5inch-e-paper.htm). (recommended)                                |
  | Good Display 7.5in e-paper (GDEY075T7)  | 800x480px  | Black/White     | [Temporarily Unavailable](https://www.aliexpress.com/item/3256802683908868.html)? (wrong product listed?)             |
  | Waveshare 7.5in e-Paper (B)             | 800x480px  | Red/Black/White | Available [here](https://www.waveshare.com/product/7.5inch-e-paper-b.htm).                                            |
  | Good Display 7.5in e-paper (GDEY075Z08) | 800x480px  | Red/Black/White | Available [here](https://www.aliexpress.com/item/3256803540460035.html).                                              |
  | Waveshare 7.3in ACeP e-Paper (F)        | 800x480px  | 7-Color         | Available [here](https://www.waveshare.com/product/displays/e-paper/epaper-1/7.3inch-e-paper-f.htm).                  |
  | Good Display 7.3in e-paper (GDEY073D46) | 800x480px  | 7-Color         | Available [here](https://www.aliexpress.com/item/3256805485098421.html).                                              |
  | Waveshare 7.5in e-paper (v1)            | 640x384px  | Black/White     | Limited support. Some information not displayed, see [image](showcase/demo-waveshare75-version1.jpg).                 |
  | Good Display 7.5in e-paper (GDEW075T8)  | 640x384px  | Black/White     | Limited support. Some information not displayed, see [image](showcase/demo-waveshare75-version1.jpg).                 |

  This software has limited support for accent colors. E-paper panels with additional colors tend to have longer refresh times, which will reduce battery life.

### Enclosure

## Setup Guide

### Wiring

Wiring is specific for Lolin D32 board:

<!-- The battery can be charged by plugging the FireBeetle ESP32 into the wall via the USB-C connector while the battery is plugged into the ESP32's JST connector.

  > **Warning**
  > The polarity of JST-PH2.0 connectors is not standardized! You may need to swap the order of the wires in the connector.

NOTE: Waveshare now ships revision 2.3 of their e-paper HAT (no longer rev 2.2 ). Rev 2.3 has an additional `PWR` pin (not depicted in the wiring diagrams below); connect this pin to 3.3V.

IMPORTANT: The DESPI-C02 adapter has one physical switch that MUST be set correctly for the display to work.

- RESE: Set switch to position 0.47.

IMPORTANT: The Waveshare E-Paper Driver HAT has two physical switches that MUST be set correctly for the display to work.

- Display Config: Set switch to position B.

- Interface Config: Set switch to position 0.

Cut the low power pad for even longer battery life.

- From <https://wiki.dfrobot.com/FireBeetle_Board_ESP32_E_SKU_DFR0654>

  > Low Power Pad: This pad is specially designed for low power consumption. It is connected by default. You can cut off the thin wire in the middle with a knife to disconnect it. After disconnection, the static power consumption can be reduced by 500 μA. The power consumption can be reduced to 13 μA after controlling the maincontroller enter the sleep mode through the program. Note: when the pad is disconnected, you can only drive RGB LED light via the USB Power supply. -->

![Wiring diagram with DESPI-C02 driver board.](showcase/wiring_diagram_despi-c02.png)


### Configuration, Compilation, and Upload

PlatformIO for VSCode is used for managing dependencies, code compilation, and uploading to ESP32.

1. Clone this repository or download and extract the .zip.

2. Install VSCode.

3. Follow these instructions to install the PlatformIO extension for VSCode: <https://platformio.org/install/ide?install=vscode>

4. Open the project in VSCode.

   a. File > Open Folder...

   b. Navigate to this project and select the folder called "platformio".

5. Configure Options.

   - The configuration options and their defaults are listed in [configschema.py](platformio/python/configschema.py), to modify them edit the [config.json](platformio/config.json) file.

   - Important settings to configure in config.json:

     - WiFi credentials (ssid, password).

     - Open Weather Map API key (only for Open Weather Map, it's free, see next section for important notes about obtaining an API key).

     - Latitude and longitude.

     - Time and date formats.

     - Sleep duration.

     - Units (Metric or Imperial).

   - (TODO - Comments explain each option in detail).

6. Build and Upload Code.

   a. Connect ESP32 to your computer via USB.

   b. Click the upload arrow along the bottom of the VSCode window. (Should say "PlatformIO: Upload" if you hover over it.)

      - PlatformIO will automatically download the required third-party libraries, compile, and upload the code. :)

      - You will only see this if you have the PlatformIO extension installed.

      - If using a FireBeetle 2 ESP32-E and you receive the error `Wrong boot mode detected (0x13)! The chip needs to be in download mode.` unplug the power from the board, connect GPIO0 ([labeled 0/D5](https://wiki.dfrobot.com/FireBeetle_Board_ESP32_E_SKU_DFR0654#target_5)) to GND, and power it back up to put the board in download mode.

      - If you are getting other errors during the upload process, you may need to install drivers to allow you to upload code to the ESP32.

### OpenWeatherMap API Key

Sign up here to get an API key; it's free. <https://openweathermap.org/api>

This project will make calls to 2 different APIs ("One Call" and "Air Pollution").

- The One Call API 3.0 is only included in the "One Call by Call" subscription. This separate subscription includes 1,000 calls/day for free and allows you to pay only for the number of API calls made to this product.

Here's how to subscribe and avoid any credit card changes:
   - Go to <https://home.openweathermap.org/subscriptions/billing_info/onecall_30/base?key=base&service=onecall_30>
   - Follow the instructions to complete the subscription.
   - Go to <https://home.openweathermap.org/subscriptions> and set the "Calls per day (no more than)" to 1,000. This ensures you will never overrun the free calls.

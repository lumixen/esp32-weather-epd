## What's is different
- Adjusted specifically for Lolin D32 board.
- Open-Meteo is the main weather API. No need to provide the credit card details to get the OWM token.
- Configuration moved to a non-versioned JSON file which is processed by a Python script to generate C++ defines.
- Added support for DKE DEPG0750RWF86BF display.
- Removed BME integration.

---

# ESP32 E-Paper Weather Display

<p float="left">
  <img src="https://github.com/user-attachments/assets/15d49106-3b07-4fbe-b252-1a642cb1251c" width="49%" />
  <img src="https://github.com/user-attachments/assets/d7e9c4d2-8885-43cd-aa22-df5e04820dd2" width="49%" />
</p>

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

## Setup Guide

### Wiring

Wiring is specific for Lolin D32 board:

### Configuration, Compilation, and Upload

Here's how to subscribe and avoid any credit card changes:
   - Go to <https://home.openweathermap.org/subscriptions/billing_info/onecall_30/base?key=base&service=onecall_30>
   - Follow the instructions to complete the subscription.
   - Go to <https://home.openweathermap.org/subscriptions> and set the "Calls per day (no more than)" to 1,000. This ensures you will never overrun the free calls.

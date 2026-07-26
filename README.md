# CYD (ESP32-2432S028) Touchscreen BLE Media Remote

A modern, high-performance **Bluetooth LE HID Media Remote** built for the **ESP32 Cheap Yellow Display (CYD / ESP32-2432S028)**. Controls music playback (Play/Pause, Next Track, Previous Track, Volume Up, Volume Down) directly from a 2.8" TFT touchscreen with **live Play/Pause state synchronization via Apple Media Service (AMS)**.

![CYD Touch Remote](https://raw.githubusercontent.com/username/cyd-ble-touch-remote/main/docs/cyd_remote.jpg)

## ✨ Features

- 📱 **Full Touch Media Control:** Large 5-button responsive touchscreen interface.
- 🎵 **Live Play/Pause Status Sync:** Dynamically switches icon between `▶` (Play) and `❚❚` (Pause) based on real-time iOS/macOS/Android media state via AMS.
- ⚡ **Zero Bus Collision Architecture:** Dedicated SPI bus for XPT2046 resistive touch controller (`CLK=25, MISO=39, MOSI=32, CS=33`) prevents display SPI interference.
- 🖤 **Minimalist Neon Black Theme:** Pitch-black background with vibrant Electric Blue & Cyan double-outline borders.
- 🔗 **Automatic Pairing & Reconnection:** Emulates Apple HID Consumer Control headset appearance (`0x0942`) for instant BLE auto-connect.
- 🛡️ **Watchdog Protection:** Non-blocking asynchronous BLE GAP handlers prevent watchdog timeouts.

## 🛠️ Hardware Requirements

- **ESP32 CYD Board** (ESP32-2432S028 - 2.8" 320x240 ILI9341 LCD + XPT2046 Touch)
- Micro USB / USB-C Cable

## 📐 Pin Mapping (CYD ESP32-2432S028)

| Component | Pin Function | ESP32 GPIO Pin |
|---|---|---|
| **ILI9341 Display** | TFT_MISO | GPIO 12 |
| | TFT_MOSI | GPIO 13 |
| | TFT_SCLK | GPIO 14 |
| | TFT_CS | GPIO 15 |
| | TFT_DC | GPIO 2 |
| | TFT_BL (Backlight) | GPIO 21 |
| **XPT2046 Touch** | TOUCH_MISO | GPIO 39 |
| | TOUCH_MOSI | GPIO 32 |
| | TOUCH_CLK | GPIO 25 |
| | TOUCH_CS | GPIO 33 |
| | TOUCH_IRQ | GPIO 36 |

## 🚀 Getting Started

### Using PlatformIO (Recommended)

1. Clone this repository:
   ```bash
   git clone https://github.com/YOUR_GITHUB_USERNAME/cyd-ble-touch-remote.git
   cd cyd-ble-touch-remote
   ```
2. Build and flash to your CYD board:
   ```bash
   pio run -t upload
   ```
3. Open Bluetooth settings on your iPhone / Phone / Laptop and connect to **`BT Touch Remote`**.

## 📄 License

MIT License - feel free to use, modify, and distribute for personal and commercial projects!

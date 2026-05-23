# DESK BUDDY v2.0 🤖

A smart animated desktop companion built using the **Seeed Studio XIAO ESP32S3** and a **240×240 ST7789 TFT display**.  
Desk Buddy combines expressive animated eyes, real-time clock, live weather updates, and a built-in WiFi configuration portal into a compact cyberpunk-style desk gadget.

Based on the uploaded source code.

---

# ✨ Features

- 👀 Smooth animated robotic eyes
- 🌈 Multiple eye color themes
- 💤 Auto sleep mode with dim animation
- 🕒 Real-time clock with NTP sync
- ☁️ Live weather updates using OpenWeather API
- 📅 3-step weather forecast
- 📶 Built-in WiFi setup portal
- 👆 Touch gesture controls
- ⚡ Double-buffered graphics for flicker-free animation
- 🔧 Fully customizable via browser

---

# 📸 Preview

## Main Modes

1. Animated Eyes  
2. Digital Clock  
3. Weather Screen  
4. Forecast Screen  

---

# 🛠 Hardware Used

| Component | Description |
|---|---|
| MCU | Seeed Studio XIAO ESP32S3 |
| Display | ST7789 240×240 TFT |
| Touch Input | Capacitive Touch Sensor |
| Power | USB Type-C |
| Connectivity | WiFi |

---

# 🔌 Pin Configuration

| TFT Pin | XIAO ESP32S3 Pin |
|---|---|
| SCK | GPIO7 (D8) |
| MOSI | GPIO9 (D10) |
| DC | GPIO1 (D0) |
| RST | GPIO2 (D1) |
| CS | GND |
| BLK | 3.3V |
| Touch OUT | GPIO3 (D2) |

---

# 🎮 Touch Controls

| Action | Function |
|---|---|
| Single Tap | Switch page |
| Double Tap | Change eye color |
| Long Press | Toggle eye shape |

---

# 🌐 WiFi Configuration Portal

If WiFi connection fails, Desk Buddy automatically starts a setup portal.

## Access Point Details

| Setting | Value |
|---|---|
| SSID | `DeskBuddy-Setup` |
| Password | `12345678` |

## Setup Steps

1. Connect to the WiFi network
2. Open browser
3. Visit:

```txt
192.168.4.1
```

4. Enter:
   - WiFi credentials
   - OpenWeather API key
   - City & country
   - Timezone offset

---

# ☁️ Weather API Setup

Desk Buddy uses the OpenWeather API.

Get your free API key from:

https://openweathermap.org/api

---

# 📚 Required Arduino Libraries

Install these libraries from the Arduino Library Manager:

- LovyanGFX
- Arduino_JSON
- WiFi
- WebServer
- DNSServer
- HTTPClient
- Preferences

---

# 🚀 Installation

## 1. Install Arduino IDE

Download:

https://www.arduino.cc/en/software

---

## 2. Install ESP32 Board Package

Add this URL in Arduino IDE:

```txt
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Then install:
- ESP32 by Espressif Systems

---

## 3. Select Board

```txt
XIAO_ESP32S3
```

---

## 4. Upload Code

1. Open the `.ino` file
2. Connect XIAO ESP32S3
3. Select COM port
4. Click Upload

---

# 🧠 Software Architecture

## Main Functional Modules

- Display Driver Initialization
- Sprite Rendering Engine
- Touch Gesture System
- Weather Fetch System
- WiFi Config Portal
- Eye Animation Engine
- Power Saving Logic
- Multi-page UI Renderer

---

# 🎨 UI Pages

## 👀 Eyes Mode
- Animated pupils
- Random blinking
- Sleep dimming
- Shape toggle

## 🕒 Clock Mode
- 12-hour format
- AM/PM indicator
- Date display

## ☁️ Weather Mode
- Temperature
- Humidity
- Weather conditions
- Dynamic icons

## 📅 Forecast Mode
- Upcoming temperatures
- Forecast labels

---

# ⚡ Performance Optimizations

- DMA-based sprite rendering
- Double buffering
- Partial redraw logic
- Optimized animation loop
- 50 FPS capped rendering

---

# 🔮 Future Improvements

- Voice assistant integration
- Bluetooth app control
- Custom eye expressions
- OTA firmware updates
- Sound effects
- RGB ambient lighting
- AI chatbot integration

---

# 🧪 Tested On

- Arduino IDE 2.x
- ESP32 Board Package v3.x
- XIAO ESP32S3
- ST7789 240×240 TFT

---

# 📂 Project Structure

```txt
DeskBuddy/
│
├── DeskBuddy.ino
├── README.md
└── assets/
```

---

# 🏆 Learning Outcomes

This project demonstrates:

- Embedded systems programming
- ESP32 firmware development
- SPI display interfacing
- Real-time graphics rendering
- WiFi networking
- REST API integration
- Human-machine interaction
- UI/UX for embedded devices

---

# 📜 License

This project is open-source and free to modify for personal and educational use.

---

# 👨‍💻 Author

**Shreyas Shivale**  
Electronics & Telecommunication Engineering

---

# ⭐ If You Like This Project

Give it a ⭐ on GitHub and share it with others!

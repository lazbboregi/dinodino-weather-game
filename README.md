# DINODINO - Weather Display & Dino Game

A weather station and endless-runner dino game built on an ESP32-C3, 
featuring a touchscreen UI and multiple animated "worlds" (Earth, Alien, Hell).

## Hardware

- **Microcontroller:** ESP32-C3-DevKitM-1
- **Display:** 240x280 ST7789 TFT LCD (driven via LovyanGFX)
- **Touch panel:** CST816D (I2C capacitive touch)

## Features

- Live weather data (current conditions, hourly chart, 5-day forecast) via OpenWeatherMap API
- City selection screen (4 preset cities)
- NTP-based real time clock sync
- Dino Game: an endless runner with 3 evolving worlds (Earth / Alien / Hell), 
  each with unique obstacles and backgrounds
- Touch-based navigation and jump controls

## Getting Started

### 1. Install dependencies

This project uses [PlatformIO](https://platformio.org/). Required libraries 
(declared in `platformio.ini`) will be installed automatically on first build:

- LovyanGFX
- ArduinoJson

### 2. Add your credentials

Open `src/main.cpp` and fill in these three lines near the top with your own 
WiFi and OpenWeatherMap API credentials:

```cpp
const char* ssid = "";       // <-- your WiFi network name
const char* password = "";   // <-- your WiFi password
const char* apiKey = "";     // <-- your OpenWeatherMap API key
```

You can get a free API key at [openweathermap.org](https://openweathermap.org/api).

> ⚠️ **Never commit real credentials to a public repository.** Keep these 
> fields blank in version control, and only fill them in locally on your 
> own machine before flashing the device.

### 3. Set your serial port

In `platformio.ini`, update the port to match your system:

```ini
upload_port = COM4
monitor_port = COM4
```

### 4. Build & upload

Use PlatformIO's build and upload buttons, or from the terminal:

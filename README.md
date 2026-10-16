# ESP32 Mini PFD — Multi-Mode Flight Display

A compact, ESP32-powered, round-display instrument inspired by aircraft Primary Flight Displays (PFDs), adapted for a bicycle-mounted dashboard.

The project combines real-time sensor data, a GPS receiver, and a 240 × 240 circular TFT display to create three different interfaces in one device. Each mode presents information in a different way, ranging from an aircraft-style artificial horizon to a compass and speed dashboard, and a minimalist time display.

## Table of Contents

- [Project Overview](#project-overview)
- [Features](#features)
- [Hardware Components](#hardware-components)
- [Mode 1: Artificial Horizon PFD](#mode-1-artificial-horizon-pfd)
- [Mode 2: GPS Speed and Compass Dashboard](#mode-2-gps-speed-and-compass-dashboard)
- [Mode 3: Minimalist Time Display](#mode-3-minimalist-time-display)
- [Sensor Reference](#sensor-reference)
- [Hardware Connections](#hardware-connections)
- [Software and Libraries](#software-and-libraries)
- [Mode Switching](#mode-switching)
- [Current Limitations](#current-limitations)
- [Future Improvements](#future-improvements)

---

## Project Overview

The Mini PFD is a personal electronics project that explores how aviation-inspired instrumentation can be recreated using affordable embedded hardware.

The main objective is to build a compact, self-contained display that presents useful information through a clear and intuitive interface. Inspired by the instruments found in aircraft cockpits, the project uses graphical indicators, numerical readouts, colour-coded information, and sensor measurements to communicate information efficiently on a small circular screen.

Rather than being limited to a single interface, the device is designed around three display modes, each with its own visual style and purpose.

The system is built around an ESP32 development board, which processes data from the connected sensors and updates the GC9A01 display. The design also uses individual breakout boards mounted onto a compact main PCB, with the display positioned at the front of the enclosure.

**Important:** This is an experimental bicycle display inspired by aviation instrumentation. It is not a certified aircraft instrument or a replacement for a bicycle's standard safety equipment.

## Features

- **Three display modes:** An artificial horizon, a GPS speed and compass dashboard, and a minimalist time interface.
- **Circular TFT interface:** Designed for the 240 × 240 GC9A01 round display.
- **Motion sensing:** Uses the MPU6050 to measure acceleration and angular velocity.
- **Digital compass:** Uses the HMC5883L magnetometer to estimate heading.
- **GPS integration:** Uses the GPS receiver for positioning, ground speed, and potentially GPS-derived time.
- **Pressure sensing:** Uses the BMP280 for atmospheric pressure and estimated altitude.
- **Custom graphics:** Uses drawn shapes, indicators, scales, and numerical readouts instead of relying entirely on pre-rendered images.
- **Compact hardware:** Designed to fit inside a small enclosure with a front-mounted display.

## Hardware Components

| Component | Purpose |
|---|---|
| ESP32 DevKit, 38-pin | Main microcontroller and sensor data processing |
| GC9A01, 240 × 240 | Round TFT display |
| MPU6050 | Accelerometer and gyroscope for pitch and roll estimation |
| HMC5883L | Three-axis magnetometer for compass heading |
| BMP280 | Barometric pressure measurement and estimated altitude |
| NEO-6M / compatible GPS module | GPS position, ground speed, and time data when available |
| Main PCB | Mechanical mounting and electrical interconnections |
| Enclosure | Protects the electronics and supports the display |

The GPS module should be identified by its actual hardware, since some compatible-looking boards contain different GNSS chipsets.

---

## Mode 1: Artificial Horizon PFD

**Purpose:** Display an aviation-inspired artificial horizon with attitude information and supporting navigation readouts.

### Sensors used

- **MPU6050 — Primary sensor.** Provides accelerometer and gyroscope measurements used to estimate pitch and roll.
- **HMC5883L — Heading.** Supplies magnetic field measurements used to calculate compass heading.
- **BMP280 — Altitude.** Measures atmospheric pressure, which can be converted into an estimated barometric altitude.
- **GPS module — Speed, if enabled in the integrated implementation.** Provides ground-speed information when valid GPS data is available.

### UI explanation

The main feature is a moving artificial horizon displayed within the circular screen.

- **Blue sky:** Represents the sky portion of the artificial horizon.
- **Brown ground:** Represents the ground below the horizon line.
- **Moving horizon:** The sky and ground move and rotate according to the estimated attitude.
- **Fixed yellow aircraft symbol:** Remains near the centre of the display, providing a stable reference against which the horizon moves.
- **Pitch ladder:** Numerical pitch markings help indicate the estimated nose-up or nose-down angle.
- **Roll markings and white pointer:** Provide a visual reference for the estimated bank angle.
- **SPD box:** Displays speed when the appropriate data source is integrated.
- **HDG box:** Displays heading derived from the magnetometer.
- **ALT box:** Displays estimated altitude derived from the barometric pressure sensor.

The interface follows the basic principle of an aircraft attitude indicator: the horizon moves relative to a fixed aircraft reference, rather than moving the aircraft symbol around the screen.

### How it works

The ESP32 reads accelerometer and gyroscope data from the MPU6050. Accelerometer measurements provide a gravity-based attitude reference, while gyroscope measurements help track changes in orientation between measurements. Sensor fusion is used to obtain a smoother attitude estimate.

The HMC5883L provides the magnetic measurements needed for heading, while the BMP280 estimates altitude from atmospheric pressure.

The display then converts the sensor values into graphical positions, angles, and numerical readouts.

**Note:** The MPU6050 does not directly measure absolute orientation. Gyroscope drift, vibration, acceleration, and sensor alignment can affect the displayed attitude. The barometric altitude is also affected by changes in atmospheric pressure.

---

## Mode 2: GPS Speed and Compass Dashboard

**Purpose:** Present speed and heading through a compact, modern dashboard rather than a traditional artificial horizon.

### Sensors used

- **GPS module:** Supplies ground speed and GPS status. It can also provide position and time information.
- **HMC5883L:** Supplies magnetic field measurements for compass heading.

The MPU6050 and BMP280 are not required for the core speed-and-compass interface unless additional functions are integrated into this mode.

### UI explanation

The dashboard is designed around two primary pieces of information: speed and direction.

- **Speed readout:** Displays the numerical speed prominently, with the `km/h` unit directly below it.
- **Speed colour track:** A horizontal or curved indicator uses a grey baseline with a coloured gradient revealed progressively by a marker.
- **Colour progression:** Green, yellow, and red distinguish different portions of the configured speed scale.
- **Speed marker:** A light-grey pin indicates the current position on the speed track.
- **Compass heading ladder:** A scrolling directional scale communicates the current heading.
- **Fixed heading indicator:** A stationary triangle and numerical heading readout make it easier to read direction while the compass scale moves.
- **GPS status icon:** Indicates the receiver's current data status, helping distinguish a functioning GPS data stream from a receiver that is not providing valid NMEA messages.

The speed scale is configured around a range of 0–60 km/h, with colour thresholds used to distinguish lower, middle, and higher speeds.

The colours are visual indicators for the configured display scale, not independently validated safety thresholds.

### How it works

The ESP32 receives GPS messages through its UART interface and parses the available navigation information. When valid ground-speed data is available, the speed readout and marker can be updated.

The HMC5883L provides magnetic field measurements, which are processed into a heading for the compass interface.

GPS status and GPS fix status are different:

- **No valid NMEA data:** The module may be disconnected, unresponsive, or transmitting data that the software cannot parse.
- **Valid NMEA data without a GPS fix:** The receiver is communicating, but a reliable position or GPS-derived speed may not yet be available.
- **Valid GPS fix:** The receiver has acquired a navigation solution, allowing the software to use the corresponding navigation data.

A compass heading calculated from the HMC5883L is independent of GPS lock, although it requires appropriate calibration and correction for the sensor's orientation.

---

## Mode 3: Minimalist Time Display

**Purpose:** Provide a simplified, information-focused screen with the time as its main visual element.

### Sensors and data sources

- **GPS module — Intended time source.** A GNSS receiver can supply UTC time, which can be converted to local time when the receiver has valid time information.
- **BMP280 — Optional altitude source.** Can supply estimated barometric altitude if connected to and read by this mode.
- **Temperature data — Requires a suitable source.** The BMP280 measures temperature internally, but its temperature reading reflects the sensor's local environment and is not necessarily an accurate ambient temperature measurement.

The time-display prototype has also used placeholder values for its numerical readouts. A displayed value should not be assumed to be live sensor data until the corresponding sensor-reading code is connected.

### UI explanation

The interface uses a minimalist arrangement with a clear visual hierarchy.

- **Large central time:** The most prominent element, intended to be readable at a glance.
- **Smaller AM/PM indicator:** Sits below or beside the time to distinguish morning and afternoon.
- **Temperature section:** Uses a thermometer-style icon and a numerical temperature readout.
- **Altitude section:** Uses a mountain-style icon and an altitude readout in metres.
- **Circular layout:** Keeps the information grouped within the round display's limited screen area.

The design intentionally gives the time the most visual emphasis while keeping supporting measurements smaller.

### How it works

The ESP32 renders the time and supporting information using the GC9A01 display. If GPS-derived time is implemented, the software can use the receiver's UTC time and apply Singapore's UTC+8 offset to display local time.

A GPS position fix is not always strictly necessary for a receiver to output time, but the receiver must have acquired valid time information. The implementation should check the actual receiver data rather than assuming that time is available whenever the GPS module is connected.

If no valid time source is available, the interface should indicate that the time is unavailable or use a clearly defined alternative timekeeping method.

---

## Sensor Reference

| Sensor | Measures | Main use |
|---|---|---|
| MPU6050 | Acceleration and angular velocity | Artificial horizon, pitch and roll estimation |
| HMC5883L | Magnetic field along three axes | Compass heading |
| BMP280 | Atmospheric pressure and sensor temperature | Estimated barometric altitude and optional temperature readout |
| GPS receiver | GNSS navigation messages | Ground speed, position, receiver status, and time when available |

### Shared sensor considerations

The I²C sensors share the ESP32's I²C bus. Each sensor is addressed individually by the software.

The GPS receiver uses a serial UART connection, allowing navigation messages to be received independently of the I²C bus.

Not every mode needs every sensor. Keeping the interfaces separate allows each mode to use only the measurements relevant to its function.

---

## Hardware Connections

The following pin assignments reflect the current hardware configuration.

### GC9A01 display

| Display pin / signal | ESP32 GPIO |
|---|---:|
| CS | 5 |
| DC | 27 |
| RST | 26 |
| SCLK | 18 |
| MOSI | 23 |

The display uses SPI. These assignments should match the selected TFT_eSPI configuration.

### I²C sensors

| Signal | ESP32 GPIO |
|---|---:|
| SDA | 21 |
| SCL | 22 |

The MPU6050, HMC5883L, and BMP280 share the I²C bus, provided their addresses do not conflict and the electrical connections are compatible.

Typical addresses used by the current modules are:

- MPU6050: `0x68`
- HMC5883L: `0x1E`
- BMP280: `0x76` or `0x77`

Actual addresses depend on the breakout board and its configuration.

### GPS receiver

| GPS signal | ESP32 connection |
|---|---|
| GPS TX | GPIO 16 (ESP32 RX) |
| GPS RX | GPIO 17 (ESP32 TX) |
| GND | Common ground |
| VCC | Compatible regulated supply |

GPS TX must connect to the ESP32's receiving pin, and GPS RX connects to the ESP32's transmitting pin.

Check the specific GPS breakout board's supply requirements and logic levels before connecting it. A compatible power supply does not necessarily guarantee that every signal pin is 5 V tolerant.

---

## Software and Libraries

The firmware is developed for the Arduino framework on the ESP32.

### Main libraries

- **TFT_eSPI:** Controls the GC9A01 display and provides drawing functions and sprite support.
- **SPI:** Handles display communication over SPI.
- **Wire:** Provides I²C communication with the motion, compass, and pressure sensors.
- **HardwareSerial:** Receives GPS data over the ESP32 UART.
- **Math and standard C/C++ utilities:** Support calculations, coordinate transformations, formatting, and graphical rendering.

Additional libraries may be required depending on the specific sensor drivers and the implementation used in each sketch.

### Display rendering

The user interfaces are drawn using graphics primitives, numerical text, and custom layout functions. The artificial horizon uses a sprite-based rendering approach to reduce flickering and improve the smoothness of the animation.

The three modes are maintained as separate sketches during development so that their displays and sensor functions can be tested independently before being combined into a single firmware project.

---

## Mode Switching

The intended combined device supports switching between the three interfaces using a physical control.

A tactile push button can cycle through the modes in sequence:

1. Artificial Horizon PFD
2. GPS Speed and Compass Dashboard
3. Minimalist Time Display
4. Return to Artificial Horizon PFD

The firmware should maintain a mode variable and use a button-press event to advance it. Button debouncing is required to prevent one physical press from being registered as multiple presses.

A single push button is suitable for sequential cycling. An SPDT switch, by contrast, normally provides two stable positions, so selecting three modes requires additional switching logic or a different control arrangement.

For a combined implementation, shared hardware initialisation and sensor-reading functions should be separated from the mode-specific drawing functions. This reduces duplicated code and makes it easier to maintain all three interfaces.

---

## Current Limitations

The following points should be considered when using or extending the project:

- **GPS reception:** A receiver may output valid NMEA messages without having a position fix. Indoor reception, antenna placement, and signal obstruction can affect performance.
- **GPS hardware identification:** Compatible-looking GPS boards may contain different chipsets, so baud rates, message formats, and supported features should be verified.
- **Compass calibration:** Magnetic interference from wiring, metal components, and nearby electronics can affect heading accuracy.
- **Artificial horizon drift:** Gyroscope bias and dynamic acceleration can affect pitch and roll estimates.
- **Barometric altitude:** The BMP280 estimates altitude from atmospheric pressure, so pressure changes can cause the displayed altitude to drift.
- **Placeholder values:** Some numerical values in prototype screens may still be static examples rather than live sensor readings.
- **Mode integration:** Individual sketches must be combined carefully so that display configuration, sensor initialisation, serial communication, and timing do not conflict.

## Future Improvements

Potential improvements include:

- Combine the three standalone sketches into one firmware application.
- Implement reliable button debouncing and mode-state management.
- Add a shared sensor service so each mode can access updated sensor values.
- Improve compass calibration and heading compensation.
- Add GPS signal and fix indicators with clearer error handling.
- Replace all remaining placeholder values with validated live measurements.
- Improve startup calibration and recovery from sensor errors.
- Refine power management and optimise display update rates.
- Complete the compact PCB and enclosure for a more robust final build.

---

## Project Summary

The ESP32 Mini PFD combines embedded programming, sensor integration, graphical interface design, and compact electronics into one multi-mode instrument.

Its three interfaces demonstrate different approaches to presenting information: an aviation-inspired artificial horizon, a navigation-oriented speed and compass dashboard, and a minimalist time display.

The project provides a foundation for exploring real-time sensor processing, embedded graphics, GPS communication, and the design of readable interfaces on small circular displays.

**Built with an ESP32, a GC9A01 round TFT, and a collection of motion, magnetic, pressure, and GPS sensors.**

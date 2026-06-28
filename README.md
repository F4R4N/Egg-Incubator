# 🥚 Arduino Egg Incubator Controller

An Arduino-based egg incubator system that monitors and regulates **temperature**, **humidity**, and **CO₂** levels to maintain optimal hatching conditions. Designed to be simulated in **Proteus** and deployed on Arduino Uno hardware, it features a 20×4 LCD menu interface, EEPROM-persisted user settings, and PWM-driven actuators.

---

## 📋 Table of Contents

- [About](#about)
- [Features](#features)
- [Hardware & Pins](#hardware--pins)
- [Simulation Platform](#simulation-platform)
- [How It Works](#how-it-works)
- [Menu Navigation](#menu-navigation)
- [Getting Started](#getting-started)
- [Dependencies](#dependencies)
- [Project Structure](#project-structure)
- [Known Limitations](#known-limitations)
- [License](#license)

---

## 🐣 About

Egg incubation requires tight environmental control across the full incubation period. This system automates that process by continuously monitoring and adjusting the three critical parameters:

| Parameter | Typical Target |
|-----------|---------------|
| Temperature | ~37.5 °C (99.5 °F) |
| Humidity | ~55–60% (rising to ~65–70% at lockdown) |
| CO₂ / Ventilation | Controlled via motorized vent lid |

The controller reads sensor values, drives the appropriate actuators, and lets the user fine-tune all targets through an on-device LCD menu — no reflashing required.

---

## ✨ Features

- **Temperature Control** — Dual-mode regulation using a heater and a radiator electric valve, both driven by PWM duty-cycle logic scaled proportionally to the deviation from the target temperature.
- **Humidity Control** — A multi-stage state machine cycles the water pump and fan: water + wind on (5 s) → wind only (2 s) → rest (1 s) → re-check, repeating until the target humidity is reached.
- **CO₂ Control** — CO₂ level is received via Serial input and used to drive a motorized vent lid to a calculated openness percentage, checked and updated every 20 seconds.
- **Light Control** — A dedicated button turns the light on for a fixed 12-second burst.
- **20×4 LCD Interface** — Displays live readings for temperature, humidity, CO₂, and system state. Navigable via a resistor-ladder keypad (UP / DOWN / OK / LIGHT buttons on a single analog pin).
- **Settings Menu** — An in-system menu lets you set preferred temperature, humidity, and CO₂ targets without reflashing.
- **EEPROM Persistence** — All preferred values are saved to EEPROM and restored on power-up, surviving resets and power cycles.
- **Non-blocking Input** — Key debouncing and release detection are handled without `delay()`, keeping all control loops running smoothly.
- **Power Switch** — A hardware power pin puts the system into an idle state, turning off all actuators and hiding the menu until power is restored.

---

## 🔌 Hardware & Pins

| Pin | Role |
|-----|------|
| `A0` | Keypad (resistor ladder — UP / DOWN / OK / LIGHT) |
| `A1` | Humidity sensor (analog) |
| `A2` | Temperature sensor (analog) |
| `A3` | Potentiometer (lid openness feedback) |
| `A4` | Open lid motor output |
| `A5` | Close lid motor output |
| `2` | Heater relay |
| `3` | Radiator electric valve |
| `10` | Humidity water pump |
| `11` | Power switch (INPUT_PULLUP) |
| `12` | Humidity fan |
| `13` | Light output |
| `4–9` | LCD (D4, D5, D6, D7, RS, EN) |

---

## 🖥️ Simulation Platform

This project is designed to be built and simulated in **[Proteus Design Suite](https://www.labcenter.com/)**.

### Recommended Setup

Open Proteus and import the compiled hex file into the arduino module

## ⚙️ How It Works

### Temperature

The heater and radiator valve each run on a **10-second PWM window**. The duty cycle is calculated proportionally from the gap between the current and target temperature — the heater activates when it's too cold, the valve when it's too hot.

### Humidity

A **state machine** with 4 states manages the humidifier:
- **State 0** — Idle; checks if humidity is below target.
- **State 1** — Water pump + fan ON for 5 seconds.
- **State 2** — Fan only ON for 2 seconds (water off).
- **State 3** — All off for 1 second, then re-evaluates.

### CO₂ / Lid

Every 20 seconds, the system calculates a **preferred lid openness** from the difference between the current and target CO₂ levels. A potentiometer on `A3` provides real-time lid position feedback, and the open/close motor pins are driven accordingly until the lid reaches the target position (within ±5%).

---

## 🗂️ Menu Navigation

The menu is accessed by pressing **OK** from the main display.

```
> Set Temperature
  Set Humidity
  CO2 Menu
  Exit
```

- **UP / DOWN** — Move cursor.
- **OK** — Enter selection.
- Inside an edit screen, **UP / DOWN** increment/decrement the value, and **OK** saves it to EEPROM.
- The CO₂ edit screen additionally shows live CO₂ readings and current lid openness.

---

## 🚀 Getting Started

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) (1.8+ or 2.x)
- [Proteus Design Suite](https://www.labcenter.com/) (for simulation)
- `LiquidCrystal` library (built into Arduino IDE)
- `EEPROM` library (built into Arduino IDE)

### Steps

1. Clone this repository:
   ```bash
   git clone https://github.com/your-username/your-repo-name.git
   ```
2. Open the `.ino` file in Arduino IDE.
3. Select **Board: Arduino Uno** and compile.
4. For Proteus: load the generated `.hex` into the Arduino component.
5. For real hardware: upload directly via USB.

---

## 📦 Dependencies

| Library | Source |
|---------|--------|
| `LiquidCrystal` | Arduino built-in |
| `EEPROM` | Arduino built-in |

No external libraries required.

---

# auto-irrigation-system
# 🌿 Professional Automated Irrigation System (ATmega32)

## 📌 Project Overview
This project is an advanced automated plant irrigation system built on the **AVR ATmega32** microcontroller. Unlike simple delay-based scripts, this system implements a **Finite State Machine (FSM)** and a non-blocking **Timer1-based millisecond counter** to ensure high reliability and safety.

## ⚙️ Key Features
* **Non-blocking Logic:** Uses a custom `millis()` implementation with Timer1 for precise timing without halting the CPU.
* **State Machine Architecture:** System transitions between `READING`, `PUMPING`, `ABSORPTION`, and `ERROR` states.
* **Safety Fail-safes:** * **Reservoir Check:** Stops the pump immediately if the water tank is empty.
    * **Anti-Flood Protection:** If soil moisture doesn't improve after 5 cycles, the system enters an `ERROR` state to prevent water waste or pump damage.
* **Visual Feedback:** Dedicated LED patterns for different system statuses (pumping, error, low water).

## 🛠️ Hardware Mapping (Pinout)
| Component | ATmega32 Pin | Logic |
| **Soil Moisture Sensor** | PA0 (ADC0) | Analog (0-1023) |
| **Water Pump (Relay)** | PB0 | Digital (Active HIGH) |
| **Status LED** | PB1 | Digital (PWM/Blink) |
| **Water Level Sensor** | PD2 | Digital (Active LOW) |

## 📁 Project Structure
- `/src`: Contains the `main.c` file with the core logic.
- `/proteus`: Contains the `.pdsprj` simulation file and the compiled `.hex` file.
- `/img`: Schematic screenshots and circuit diagrams.

## 📋 System States
1.  **READING:** Monitors moisture levels every 15 seconds.
2.  **PUMPING:** Activates the pump for 5 seconds if moisture is < 30%.
3.  **ABSORPTION:** Waits 10 seconds for the water to settle before re-checking.
4.  **ERROR:** Safety lockout if the system detects a failure or lack of progress.

## 🚀 Getting Started
1.  **Simulation:** Open the Proteus file in `/proteus`.
2.  **Firmware:** The code is configured for a **16 MHz** clock. Load the provided `.hex` file onto the ATmega32.
3.  **Compiling:** If you wish to modify the code, use **Microchip Studio** or **AVR-GCC**. Ensure `F_CPU` is defined as `16000000UL`.

## 💻 Technical Details
- **MCU:** ATmega32
- **Clock:** 16 MHz External/Internal
- **Language:** C (Standard AVR Libs)

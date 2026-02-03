# Arduino Smart Home Security + Fire Detection (Bluetooth Controlled)

This project is an Arduino Uno based **home security + fire/smoke detection** system with:
- **Password door unlocking** (4×4 keypad + 16×2 I2C LCD + servo door/gate)
- **Fire/Smoke detection** (Flame sensor + MQ-2 smoke sensor)
- **Emergency response** (buzzer + LED + relay controlled motor/pump)
- **Bluetooth control from mobile phone** (open/close gate, motor ON/OFF, stop wrong-pin buzzer, full reset)

Bluetooth commands are sent using any **Bluetooth Serial Terminal** app connected to an HC-05/HC-06 module.

---

## Features
### Security (Normal Mode)
- LCD prompts: `Enter Pass:`
- User enters 4 digits → LCD shows `****`
- Correct password (`1234`) → door/gate opens (servo)
- Door auto-locks after **60 seconds**
- Wrong password increases attempts; after **3** wrong attempts system becomes **LOCKED** and intruder alarm starts

### Fire / Smoke Alert Mode (Highest Priority)
- Flame sensor + MQ-2 digital output are monitored
- When confirmed hazard occurs, system enters **FIRE ALERT**:
  - Relay motor/pump forced **ON**
  - Buzzer + LED blink continuously
  - LCD shows `!!! FIRE ALERT !!!`
- Fire mode exits only when hazard stays cleared for a few seconds (stable confirmation)

### Bluetooth Mobile Control
Using a phone Bluetooth serial app:
- Open gate / close gate
- Motor (pump/fan) ON / OFF
- Stop **wrong-pin alarm** (intruder buzzer) without unlocking
- Full system reset

---

## Hardware / Components
- Arduino Uno R3
- 16×2 LCD with I2C (address used: **0x27**)
- 4×4 Matrix Keypad
- Servo motor for **door/gate** (signal on D5)
- Servo motor for **sweep/scan** (signal on D6)
- Flame sensor module (digital output)
- MQ-2 gas/smoke sensor module (digital output used; analog optional)
- Relay module (Active LOW) controlling motor/pump
- Buzzer + LED indicator
- Bluetooth module **HC-05 / HC-06** (Serial)
- Breadboard + jumper wires
- External supply for motor/pump (recommended)

---

## Pin Mapping (Arduino Uno)
| Module | Pin |
|---|---|
| Flame Sensor (Digital) | D9 |
| MQ-2 Sensor (Digital) | D10 |
| MQ-2 Analog (optional) | A2 |
| Buzzer | D3 |
| LED | A3 |
| Relay IN (Motor/Pump) | D11 |
| Door/Gate Servo | D5 |
| Sweep Servo | D6 |
| Keypad Rows | A1, A0, D2, D4 |
| Keypad Cols | D7, D8, D12, D13 |
| LCD I2C | SDA=A4, SCL=A5 |

Relay logic in code:
- `FAN_ON  = LOW`
- `FAN_OFF = HIGH`

---

## Bluetooth Setup (Mobile Control)
### Bluetooth Module Wiring (HC-05 / HC-06)
**Recommended:**
- HC-05 VCC → 5V
- HC-05 GND → GND
- HC-05 TXD → Arduino RX (D0)
- HC-05 RXD → Arduino TX (D1) *(use a voltage divider for safety)*

⚠️ If using D0/D1, disconnect HC-05 from RX/TX when uploading code to Arduino.

### Mobile App
Use any **Bluetooth Serial Terminal** style app:
- Pair with HC-05 (commonly PIN `1234` or `0000`)
- Set baud rate: **9600**
- Send single character commands (no need for Enter)

---

## Bluetooth Command List
| Command | Action |
|---|---|
| `o` | Open gate/door (servo to OPEN position) |
| `c` | Close gate/door (servo to CLOSED position) |
| `f` | Motor/Pump ON (relay ON) |
| `s` | Motor/Pump OFF (relay OFF) |
| `b` | Stop **wrong-pin** buzzer/LED (still locked if lockedOut) |
| `r` | Full system reset |

Notes:
- Fire mode has priority: when fire/smoke confirmed, relay is forced ON and buzzer/LED blink.
- `b` is intended for **intruder/wrong-pin alarm silence**, not for fire safety.

---

## How the System Works (Process / Flow)
1. **Power ON**
   - LCD shows `Enter Pass:`
   - Bluetooth is active immediately
   - Sensors are ignored for the first **2 seconds** to avoid instant false alarms after upload

2. **Normal Mode**
   - User can unlock via keypad OR open/close via Bluetooth
   - Sweep servo runs continuously (40° ↔ 120°) to demonstrate scanning motion

3. **Wrong Password / Lockout**
   - After 3 wrong passwords:
     - `LOCKED!` on LCD
     - Intruder alarm activates (buzzer + blinking LED)
   - Bluetooth command `b` can silence buzzer/LED but the system may remain locked
   - Bluetooth command `r` resets system

4. **Fire Mode**
   - Flame sensor / MQ-2 detection is confirmed (stable)
   - System enters FIRE ALERT:
     - motor/pump ON via relay
     - buzzer + LED blinking
     - sweep servo stops (to prioritize alarm handling)
   - Fire clears only after hazard stays OFF for a few seconds, then returns to normal screen

---

## Software / Libraries
Arduino IDE + these libraries:
- `Servo.h`
- `Wire.h`
- `LiquidCrystal_I2C.h`
- `Keypad.h`

---

## Upload & Run Steps
1. Connect Arduino Uno to PC
2. Open Arduino IDE
3. Select **Board: Arduino Uno** and correct **COM Port**
4. Upload the `.ino` file
5. Connect HC-05 to phone and send commands, or use keypad password

---

## Safety Notes
- Use a separate power supply for **servo + motor/pump** if possible (current draw is high).
- Always connect **GND common** between Arduino and external power supply.
- Do not drive a pump directly from Arduino pins; use relay/MOSFET driver.

---


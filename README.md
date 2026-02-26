&nbsp;🕒 ESP Study Clock

A \*\*feature-packed smart study clock\*\* built on the \*\*ESP32 microcontroller\*\*, designed to be both beautiful and functional.

It integrates real-time synchronization, alarms, weather, events, and multiple stunning display themes — all controllable through an intuitive user interface.

---

🎥 \*\*Demo Video:\*\* \[Watch on YouTube](https://youtu.be/ajnAWAT7DW0)

&nbsp;✨ Features

&nbsp;🕰️ Time \& Date

\- Real-time clock with \*\*NTP synchronization\*\*

\- \*\*Offline timekeeping\*\* with manual fallback

\- Multiple \*\*date/time format\*\* options (12H / 24H)

\- Customizable \*\*GMT offset\*\*

\- \*\*Week number\*\* display

\- \*\*Total days\*\* counter since first use

\- \*\*Hourly chime\*\* function with optional sound

---

&nbsp;🎨 Display Themes (13 Variants)

Choose from a wide range of layouts and visual styles:

1\. Classic Date-Time-Events

2\. Minimal Time Only

3\. Classic 2.0

4\. Minimal Inverted

5\. Classic DTE Inverted

6\. Analog Clock Face

7\. Detailed Information

8\. Boxee Layout

9\. Classic Boxee

10\. Dial Display

11\. Bar Clock Animation

12\. Classic 3.0

13\. Weather View

---

&nbsp;⏰ Alarm System

\- Supports \*\*up to 3 concurrent alarms\*\*

\- \*\*8 selectable alarm tones:\*\*

  - Basic Beep

  - Digital

  - Classic Bell

  - Chime

  - Morning Bird

  - Soft Bells

  - Gentle Rise

  - Marimba

\- Configurable \*\*snooze duration\*\*

\- \*\*Auto-delete\*\* on stop

\- Easy alarm management through menu interface

---

&nbsp;⏱️ Stopwatch

\- Full-featured stopwatch with \*\*millisecond precision\*\*

\- \*\*Two lap memory\*\* slots

\- \*\*Start / Pause / Reset\*\* functionality

\- Quick access via \*\*UP button shortcut\*\*

\- Persistent display during operation

---

&nbsp;⏲️ Timer

\- Configurable \*\*countdown timer\*\* (H:M:S)

\- \*\*Start / Pause / Resume / Reset\*\* controls

\- Quick access via \*\*DOWN button shortcut\*\*

\- \*\*Auto alert\*\* on completion

\- Saves last set duration

---

&nbsp;📅 Events Calendar

\- Loads events dynamically from \*\*SPIFFS (events.json)\*\*

\- Multiple viewing modes:

  - \*\*Today’s Events\*\*

  - \*\*All Events List\*\*

  - \*\*Monthly Calendar View\*\*

\- \*\*Scrolling text\*\* for long event names

\- \*\*Date-based organization\*\* of events

---

&nbsp;🌡️ Environmental Sensing

\- \*\*AHT10 Sensor Integration\*\*

  - Temperature display (°C)

  - Humidity monitoring (%)

\- Toggle temperature/humidity display in settings

\- Integrates with various display themes

---

⚙️ System Settings

&nbsp;🔹 WiFi Management

\- Built-in \*\*WiFiManager\*\* for easy setup

\- Displays \*\*connection status\*\*

\- \*\*Forget network\*\* function

\- \*\*Auto-reconnect\*\* on startup

&nbsp;🔹 Display Settings

\- Select from 13 \*\*themes\*\*

\- \*\*Live theme preview\*\* before applying

&nbsp;🔹 Sound Settings

\- \*\*Buzzer toggle\*\*

\- \*\*Second-hand tick\*\* sound toggle

&nbsp;🔹 Storage Management

\- \*\*SPIFFS file system\*\* for data and events

\- View \*\*storage usage\*\* and status

&nbsp;🔹 Sensor Controls

\- Enable/disable \*\*temperature\*\* and \*\*humidity\*\* sensors

---

🔩 Hardware Integration

| Component | Function |

|------------|-----------|

| \*\*ESP32 Dev Kit\*\* | Main controller (WiFi + processing) |

| \*\*128x64 I²C OLED Display\*\* | Primary user interface |

| \*\*AHT10 Sensor\*\* | Temperature \& humidity |

| \*\*Piezo Buzzer\*\* | Alarms and notifications |

| \*\*Buttons (via ADC)\*\* | Menu navigation and shortcuts |

| \*\*EEPROM\*\* | Persistent user settings |

| \*\*SPIFFS\*\* | Event and configuration storage |

---

� Circuit Diagram & Connections

**ESP32 Pinout Overview:**

```
ESP32 DEV KIT V1
┌─────────────────────┐
│    [USB]            │
│ ┌───────────────┐   │
│ │    ESP32      │   │
│ └───────────────┘   │
└─────────────────────┘
```

**Component Connections:**

| Component              | Pin       | ESP32 Pin        | Notes            |
| ---------------------- | --------- | ---------------- | ---------------- |
| **OLED Display (I²C)** | SDA       | GPIO 21          | 3.3V logic       |
|                        | SCL       | GPIO 22          | 3.3V logic       |
|                        | VCC       | 3.3V             | Power supply     |
|                        | GND       | GND              | Ground           |
| **AHT10 Sensor (I²C)** | SDA       | GPIO 21          | Same I²C bus     |
|                        | SCL       | GPIO 22          | Same I²C bus     |
|                        | VCC       | 3.3V             | Power supply     |
|                        | GND       | GND              | Ground           |
| **Piezo Buzzer**       | +         | GPIO 32          | PWM capable      |
|                        | -         | GND              | Ground           |
| **Buttons (ADC)**      | All       | GPIO 34 (ADC1_6) | Voltage divider  |
|                        | Menu      | 150Ω resistor    | 3.3V-Menu-Gnd    |
|                        | Up        | 1kΩ resistor     | Series chain     |
|                        | Down      | 1kΩ resistor     | Series chain     |
|                        | Pull-down | 10kΩ to GND      | Ground reference |

**Detailed Wiring Instructions:**

step1. D34 -- 10kΩ -- GND (first reduce noise to zero at gpio pin 34)

step2. 3.3V -- 150Ω -- [Menu Button] -- GND (for menu control)

step3. 3.3V -- 1kΩ -- [at this junction one terminal of push button be connected here and another terminal be connected to D34] -- 1kΩ -- [at this junction one terminal of push button be connected here and another terminal be connected to D34 Button] -- 1kΩ -- GND

one 150Ω for menu,

three 1kΩ for series chain to control menu,

one 10kΩ for reduce noise at D34,

Note: if you press both Up Button and Down button together then it works as Reset Button

🖥️ **OLED Display Connections:**

- Connect SDA to **GPIO 21**
- Connect SCL to **GPIO 22**
- Connect VCC to **3.3V**
- Connect GND to **GND**
- Use I²C address: **0x3C**

🌡️ **AHT10 Sensor Connections:**

- Connect SDA to **GPIO 21** (shared with OLED)
- Connect SCL to **GPIO 22** (shared with OLED)
- Connect VCC to **3.3V**
- Connect GND to **GND**
- I²C address: **0x38**

🔊 **Piezo Buzzer Connections:**

- Positive pin → **GPIO 32** (PWM output)
- Negative pin → **GND**
- Resistor: 200Ω in series recommended for current limiting
- Operating voltage: 3.3V

🔘 **Button Connections (ADC Voltage Divider Network):**

- **Network Chain:** 3.3V → 1kΩ → Button 1 → 1kΩ → Button 2 → 1kΩ → **GPIO 34** → 10kΩ → GND
- All three buttons connected in series with 1kΩ resistors each
- GPIO 34 pulled down to GND through 10kΩ resistor
- Each button press creates different voltage divider output on GPIO 34
- ADC reads voltage levels to determine which button was pressed

**Power Supply Requirements:**

- ESP32: 5V USB (with built-in regulator to 3.3V)
- OLED Display: 3.3V @ ~20mA
- AHT10 Sensor: 3.3V @ ~1mA
- Piezo Buzzer: 3.3V @ ~50mA peak
- **Total: ~5V @ 500mA recommended**

---

�🖥️ UI \& Navigation

\- Intuitive \*\*menu-based interface\*\*

\- \*\*Shortcut buttons\*\* for stopwatch/timer

\- \*\*Scrolling text\*\* for long content

\- \*\*Confirmation dialogs\*\* for critical actions

\- \*\*Status messages\*\* for user feedback

\- \*\*Battery-efficient display updates\*\*

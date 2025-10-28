# \# 🕒 ESP Study Clock

# 

# A \*\*feature-packed smart study clock\*\* built on the \*\*ESP32 microcontroller\*\*, designed to be both beautiful and functional.  

# It integrates real-time synchronization, alarms, weather, events, and multiple stunning display themes — all controllable through an intuitive user interface.

# 

# ---

# 

# \## ✨ Features

# 

# \### 🕰️ Time \& Date

# \- Real-time clock with \*\*NTP synchronization\*\*

# \- \*\*Offline timekeeping\*\* with manual fallback

# \- Multiple \*\*date/time format\*\* options (12H / 24H)

# \- Customizable \*\*GMT offset\*\*

# \- \*\*Week number\*\* display

# \- \*\*Total days\*\* counter since first use

# \- \*\*Hourly chime\*\* function with optional sound

# 

# ---

# 

# \### 🎨 Display Themes (13 Variants)

# Choose from a wide range of layouts and visual styles:

# 1\. Classic Date-Time-Events  

# 2\. Minimal Time Only  

# 3\. Classic 2.0  

# 4\. Minimal Inverted  

# 5\. Classic DTE Inverted  

# 6\. Analog Clock Face  

# 7\. Detailed Information  

# 8\. Boxee Layout  

# 9\. Classic Boxee  

# 10\. Dial Display  

# 11\. Bar Clock Animation  

# 12\. Classic 3.0  

# 13\. Weather View  

# 

# ---

# 

# \### ⏰ Alarm System

# \- Supports \*\*up to 3 concurrent alarms\*\*

# \- \*\*8 selectable alarm tones:\*\*

# &nbsp; - Basic Beep  

# &nbsp; - Digital  

# &nbsp; - Classic Bell  

# &nbsp; - Chime  

# &nbsp; - Morning Bird  

# &nbsp; - Soft Bells  

# &nbsp; - Gentle Rise  

# &nbsp; - Marimba

# \- Configurable \*\*snooze duration\*\*

# \- \*\*Auto-delete\*\* on stop

# \- Easy alarm management through menu interface

# 

# ---

# 

# \### ⏱️ Stopwatch

# \- Full-featured stopwatch with \*\*millisecond precision\*\*

# \- \*\*Two lap memory\*\* slots

# \- \*\*Start / Pause / Reset\*\* functionality

# \- Quick access via \*\*UP button shortcut\*\*

# \- Persistent display during operation

# 

# ---

# 

# \### ⏲️ Timer

# \- Configurable \*\*countdown timer\*\* (H:M:S)

# \- \*\*Start / Pause / Resume / Reset\*\* controls

# \- Quick access via \*\*DOWN button shortcut\*\*

# \- \*\*Auto alert\*\* on completion

# \- Saves last set duration

# 

# ---

# 

# \### 📅 Events Calendar

# \- Loads events dynamically from \*\*SPIFFS (events.json)\*\*

# \- Multiple viewing modes:

# &nbsp; - \*\*Today’s Events\*\*

# &nbsp; - \*\*All Events List\*\*

# &nbsp; - \*\*Monthly Calendar View\*\*

# \- \*\*Scrolling text\*\* for long event names

# \- \*\*Date-based organization\*\* of events

# 

# ---

# 

# \### 🌡️ Environmental Sensing

# \- \*\*AHT10 Sensor Integration\*\*

# &nbsp; - Temperature display (°C)

# &nbsp; - Humidity monitoring (%)

# \- Toggle temperature/humidity display in settings

# \- Integrates with various display themes

# 

# ---

# 

# \### ⚙️ System Settings

# \#### 🔹 WiFi Management

# \- Built-in \*\*WiFiManager\*\* for easy setup  

# \- Displays \*\*connection status\*\*

# \- \*\*Forget network\*\* function

# \- \*\*Auto-reconnect\*\* on startup

# 

# \#### 🔹 Display Settings

# \- Select from 13 \*\*themes\*\*

# \- \*\*Live theme preview\*\* before applying

# 

# \#### 🔹 Sound Settings

# \- \*\*Buzzer toggle\*\*

# \- \*\*Second-hand tick\*\* sound toggle

# 

# \#### 🔹 Storage Management

# \- \*\*SPIFFS file system\*\* for data and events

# \- View \*\*storage usage\*\* and status

# 

# \#### 🔹 Sensor Controls

# \- Enable/disable \*\*temperature\*\* and \*\*humidity\*\* sensors

# 

# ---

# 

# \### 🔩 Hardware Integration

# | Component | Function |

# |------------|-----------|

# | \*\*ESP32 Dev Kit\*\* | Main controller (WiFi + processing) |

# | \*\*128x64 I²C OLED Display\*\* | Primary user interface |

# | \*\*AHT10 Sensor\*\* | Temperature \& humidity |

# | \*\*Piezo Buzzer\*\* | Alarms and notifications |

# | \*\*Buttons (via ADC)\*\* | Menu navigation and shortcuts |

# | \*\*EEPROM\*\* | Persistent user settings |

# | \*\*SPIFFS\*\* | Event and configuration storage |

# 

# ---

# 

# \### 🖥️ UI \& Navigation

# \- Intuitive \*\*menu-based interface\*\*

# \- \*\*Shortcut buttons\*\* for stopwatch/timer

# \- \*\*Scrolling text\*\* for long content

# \- \*\*Confirmation dialogs\*\* for critical actions

# \- \*\*Status messages\*\* for user feedback

# \- \*\*Battery-efficient display updates\*\*

# 

# ---




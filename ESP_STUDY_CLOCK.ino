#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include "time.h"
#include <U8g2lib.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Adafruit_AHT10.h>

#define BUTTON_ADC_PIN 34
enum ButtonType { BTN_NONE,
                  BTN_MENU,
                  BTN_UP,
                  BTN_DOWN,
                  BTN_RESET };
ButtonType readButton() {
  int adc = analogRead(BUTTON_ADC_PIN);
  float adcVolt = (adc / 4095.0) * 3.3;
  if (adcVolt > 3.2) return BTN_MENU;                    // ~3.2V-3.3V
  if (adcVolt > 1.9 && adcVolt < 2.1) return BTN_UP;     // ~1.9V-2.1V
  if (adcVolt > 0.8 && adcVolt < 1.0) return BTN_DOWN;   // ~0.8V-1.0V
  if (adcVolt > 1.4 && adcVolt < 1.5) return BTN_RESET;  // ~1.4V-1.5V
  return BTN_NONE;
}

#define BUZZER_PIN 32

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_AHT10 aht;  // for temperature/humidity sensor

// -- Welcome Message
const char MSG_WELCOME[] PROGMEM = "ESP";
const char MSG_SUBTITLE[] PROGMEM = "STUDY - CLK";
const char MSG_DEVELOPER[] PROGMEM = "developed by M.Maity";

// -- Time Management
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

// -- Menu & Submenu variables
const char* menuItems[] = {
  "Back",
  "Clock Settings",
  "Alarms",
  "Stopwatch",
  "Timer",
  "Weather",
  "News",
  "Events / Calendar",
  "System Settings"
};
const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);
bool menuActive = false;
int menuIndex = 0;

/*Clock Settings*/
const char* clockSettingsMenu[] = {
  "Back",
  "Set Time & Date",
  "GMT offset",
  "Sync with NTP",
  "Time Format",
  "Date Format",
  "Day Format",
  "Week Number",
  "Total Days",
  "Daylight Saving"
};
const int clockSettingsMenuCount = sizeof(clockSettingsMenu) / sizeof(clockSettingsMenu[0]);
bool clockSettingsActive = false;
int clockSettingsIndex = 0;

/*Alarm*/
const char* alarmsMenu[] = {
  "Back",
  "Add New Alarm",
  "View Alarms",
  "Alarm Tone",
  "Snooze Duration"
};
const int alarmsMenuCount = sizeof(alarmsMenu) / sizeof(alarmsMenu[0]);
bool alarmsActive = false;
int alarmsIndex = 0;

/*Stopwatch*/
const char* stopwatchMenu[] = {
  "Back",
  "Start/Stop",
};
const int stopwatchMenuCount = sizeof(stopwatchMenu) / sizeof(stopwatchMenu[0]);
bool stopwatchActive = false;
int stopwatchIndex = 0;
bool shortcutStopwatchActive = false;
// Stopwatch state
unsigned long stopwatchStart = 0;
unsigned long stopwatchElapsed = 0;
bool stopwatchRunning = false;
unsigned long lastLapTime = 0;  // reference for interval
unsigned long lap1 = 0;         // immediate last lap
unsigned long lap2 = 0;         // one before last

/*Timer*/
const char* timerMenu[] = {
  "Back",
  "Set Duration",
  "Start/Pause"
};
const int timerMenuCount = sizeof(timerMenu) / sizeof(timerMenu[0]);
bool timerActive = false;
int timerIndex = 0;
unsigned long timerSetMillis = 0;  // Saved timer value in ms
unsigned long timerRemainingMillis = 0;
bool timerRunning = false;
bool timerPaused = false;
bool timerTimeUp = false;
bool shortcutTimerActive = false;

/*Weather*/
const char* weatherMenu[] = {
  "Back",
  "Units",
  "Update Interval",
  "Current Location",
  "Forecast"
};
const int weatherMenuCount = sizeof(weatherMenu) / sizeof(weatherMenu[0]);
bool weatherActive = false;
int weatherIndex = 0;

/*News*/
const char* newsMenu[] = {
  "Back",
  "Source Selection",
  "Headlines",
  "Refresh Interval",
  "Auto Scroll Speed"
};
const int newsMenuCount = sizeof(newsMenu) / sizeof(newsMenu[0]);
bool newsActive = false;
int newsIndex = 0;

/*Events*/
const char* eventsMenu[] = {
  "Back",
  "Today Events",
  "All Events",
  "View Month Calc."
};
const int eventsMenuCount = sizeof(eventsMenu) / sizeof(eventsMenu[0]);
bool eventsActive = false;
int eventsIndex = 0;

/*System Settings*/
const char* settingsMenu[] = {
  "Back",
  "WiFi Settings",
  "Display Settings",
  "Sounds Settings",
  "Storage",
  "Sensor",
  "About",
  "Restart"
};
const int settingsMenuCount = sizeof(settingsMenu) / sizeof(settingsMenu[0]);
bool settingsActive = false;
int settingsIndex = 0;

/*wifiMenu*/
const char* wifiMenu[] = {
  "Back",
  "Connect WiFi",
  "WiFi Status",
  "Disconnect",
  "Forget WiFi"
};
const int wifiMenuCount = sizeof(wifiMenu) / sizeof(wifiMenu[0]);
bool wifiActive = false;
int wifiIndex = 0;

/*Display Settings*/
const char* displaySettingsMenu[] = {
  "Back",
  "Themes"
};
const int displaySettingsMenuCount = sizeof(displaySettingsMenu) / sizeof(displaySettingsMenu[0]);
bool displaySettingsActive = false;
int displaySettingsIndex = 0;

//////////////////////   SETTINGS STRUCTURE   //////////////////////

// -- Clock Settings
struct ClockSettings {
  uint32_t magic = 0xDEADBEEF;  // marker to detect first run
  long gmtOffset_sec;
  int timeFormat;
  char dateFormat[16];
  char dayFormat[16];
  bool weekNumber;
  bool totalDays;
  bool daylightSaving;
};

ClockSettings clockSettings;

void saveClockSettings() {
  EEPROM.put(0, clockSettings);
  EEPROM.commit();
}

void loadClockSettings() {
  EEPROM.get(0, clockSettings);

  // If first run (magic mismatch), initialize defaults
  if (clockSettings.magic != 0xDEADBEEF) {
    clockSettings.magic = 0xDEADBEEF;
    clockSettings.gmtOffset_sec = 19800;
    clockSettings.timeFormat = 24;
    strcpy(clockSettings.dateFormat, "dd/mm/yyyy");
    strcpy(clockSettings.dayFormat, "Short");
    clockSettings.weekNumber = false;
    clockSettings.totalDays = false;
    clockSettings.daylightSaving = false;
    saveClockSettings();
  }
}
// -- Alarm
struct Alarm {
  bool active;     // Is alarm enabled
  uint8_t hour;    // Hour (0-23)
  uint8_t minute;  // Minute (0-59)
  uint8_t tone;    // Selected tone
  bool snooze;     // Is snooze enabled
};

struct AlarmSettings {
  Alarm alarms[3];         // Max 3 alarms
  uint8_t snoozeDuration;  // In minutes
  uint8_t selectedTone;    // Currently selected tone
};

AlarmSettings alarmSettings;

bool alarmTriggered = false;
int triggeredAlarmIndex = -1;
unsigned long alarmBlinkLast = 0;
bool alarmBlinkState = false;
unsigned long alarmSnoozeUntil = 0;
bool snoozeActive = false;
int snoozeHour = -1;
int snoozeMinute = -1;


const char* ALARM_TONES[] = {
  "Basic Beep",
  "Digital",
  "Classic Bell",
  "Chime",
  "Morning Bird",
  "Soft Bells",
  "Gentle Rise",
  "Marimba"
};

const int ALARM_TONES_COUNT = sizeof(ALARM_TONES) / sizeof(ALARM_TONES[0]);

void saveAlarmSettings() {
  EEPROM.put(100, alarmSettings);
  EEPROM.commit();
}
void loadAlarmSettings() {
  EEPROM.get(100, alarmSettings);

  // Detect first run / invalid data (255 means empty EEPROM)
  bool invalid = false;

  // Check snoozeDuration and tone first
  if (alarmSettings.snoozeDuration < 1 || alarmSettings.snoozeDuration > 30) {
    invalid = true;
  }
  if (alarmSettings.selectedTone >= ALARM_TONES_COUNT) {
    invalid = true;
  }

  // Check if alarms have invalid values (like 255:255)
  for (int i = 0; i < 3; i++) {
    if (alarmSettings.alarms[i].hour > 23 || alarmSettings.alarms[i].minute > 59) {
      invalid = true;
      break;
    }
  }

  if (invalid) {
    // Reset everything
    for (int i = 0; i < 3; i++) {
      alarmSettings.alarms[i].active = false;
      alarmSettings.alarms[i].hour = 0;
      alarmSettings.alarms[i].minute = 0;
      alarmSettings.alarms[i].tone = 0;
      alarmSettings.alarms[i].snooze = false;
    }
    alarmSettings.snoozeDuration = 5;  // default 5 minutes
    alarmSettings.selectedTone = 0;    // default tone
    saveAlarmSettings();
  }
}

// -- Timer
struct TimerSettings {
  int hours;
  int minutes;
  int seconds;
};

TimerSettings timerSettings;

void saveTimerSettings() {
  EEPROM.put(200, timerSettings);  // store at address 200 (safe gap from alarm)
  EEPROM.commit();
}

void loadTimerSettings() {
  EEPROM.get(200, timerSettings);
  // If first run or invalid values, reset to default
  if (timerSettings.hours < 0 || timerSettings.hours > 23 || timerSettings.minutes < 0 || timerSettings.minutes > 59 || timerSettings.seconds < 0 || timerSettings.seconds > 59) {
    timerSettings.hours = 0;
    timerSettings.minutes = 0;
    timerSettings.seconds = 0;
  }
}

// -- Sound Settings
const char* soundMenu[] = {
  "Back",
  "Buzzer",
  "Second Hand Beep"
};
const int soundMenuCount = sizeof(soundMenu) / sizeof(soundMenu[0]);
bool soundActive = false;
int soundIndex = 0;

bool buzzerActive = true;    // Default state
bool secondHandBeep = true;  //Default State

void saveBuzzerSetting() {
  EEPROM.write(300, buzzerActive ? 1 : 0);
  EEPROM.write(301, secondHandBeep ? 1 : 0);
  EEPROM.commit();
}

void loadBuzzerSetting() {
  uint8_t val = EEPROM.read(300);
  buzzerActive = (val == 1);
  uint8_t beepVal = EEPROM.read(301);
  secondHandBeep = (beepVal == 1);
}

// --- Theme Settings ---
const char* themeMenu[] = {
  "Back",
  "Classic DTE",  // date time events
  "Minimal",      // only tme
  "Classic 2.0",
  "Minimal Inverted",
  "Classic DTE Inverted",
  "Analog Dial",
  "Detailed",
  "Boxee",
  "Classic Boxee",
  "Dial",
  "Bar Clock",
  "Classic 3.0",
  "Weather View"
};
const int themeMenuCount = sizeof(themeMenu) / sizeof(themeMenu[0]);
bool themeActive = false;
int themeIndex = 0;
int selectedTheme = 0;  // 0=Theme 1, 1=Theme 2, etc.

void saveThemeSetting() {
  EEPROM.write(350, selectedTheme);
  EEPROM.commit();
}
void loadThemeSetting() {
  uint8_t val = EEPROM.read(350);
  if (val >= 0 && val < themeMenuCount - 1) selectedTheme = val;
  else selectedTheme = 0;
}

// --- Sensor Settings ---
const char* sensorMenu[] = {
  "Back",
  "Temperature",
  "Humidity",
  "Human Detector"
};
const int sensorMenuCount = sizeof(sensorMenu) / sizeof(sensorMenu[0]);
bool sensorActive = false;
int sensorIndex = 0;
bool tempSensorOn = true;
bool humidSensorOn = true;

void saveSensorSettings() {
  EEPROM.write(400, tempSensorOn ? 1 : 0);
  EEPROM.write(401, humidSensorOn ? 1 : 0);
  EEPROM.commit();
}
void loadSensorSettings() {
  tempSensorOn = EEPROM.read(400) == 1;
  humidSensorOn = EEPROM.read(401) == 1;
}


//////////////////////   SENSOR UPDATE   //////////////////////

unsigned long lastSensorRead = 0;
char sensorStr[32] = "";  // example: 35C 87%
char tempStr[16] = "";    // example: 35.7 C
char tempStr2[10] = "";   // example: 35C
char humiStr[16] = "";    // example: 87.9 %
void updateSensors() {
  if (millis() - lastSensorRead >= 5000) {  // every 2s
    lastSensorRead = millis();

    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);

    if (tempSensorOn && humidSensorOn) {
      snprintf(sensorStr, sizeof(sensorStr), "%.0fC %.0f%%", temp.temperature, humidity.relative_humidity);
      snprintf(tempStr, sizeof(tempStr), "%.1f C", temp.temperature);
      snprintf(tempStr2, sizeof(tempStr2), "%.0fC", temp.temperature);
      snprintf(humiStr, sizeof(humiStr), "%.1f %%", humidity.relative_humidity);
    } else if (tempSensorOn) {
      snprintf(sensorStr, sizeof(sensorStr), "%.0fC", temp.temperature);
      snprintf(tempStr, sizeof(tempStr), "%.1f C", temp.temperature);
      snprintf(tempStr2, sizeof(tempStr2), "%.0fC", temp.temperature);
    } else if (humidSensorOn) {
      snprintf(sensorStr, sizeof(sensorStr), "%.0f%%", humidity.relative_humidity);
      snprintf(humiStr, sizeof(humiStr), "%.1f %%", humidity.relative_humidity);
    } else {
      sensorStr[0] = '\0';
      tempStr[0] = '\0';
      tempStr2[0] = '\0';
      humiStr[0] = '\0';
    }
  }
}

//////////////////////   BUTTON CLICK SOUND   //////////////////////


void buttonClickSound() {
  if (!buzzerActive) return;
  digitalWrite(BUZZER_PIN, HIGH);
  delay(30);  // Short beep
  digitalWrite(BUZZER_PIN, LOW);
}

//////////////////////   BUTTON RELEASE   //////////////////////
void waitForButtonRelease() {
  // Wait until no button is pressed (for debounce)
  while (readButton() != BTN_NONE) {
    delay(100);
  }
  delay(100);  // Extra debounce delay
}

//////////////////////   SHOW MESSAGE   //////////////////////


void showMessage(const char* msg, int y, int delay_ms = 1000) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(0, y, msg);
  u8g2.sendBuffer();
  delay(delay_ms);
}

//////////////////////   CENTRE TEXT   //////////////////////

void drawCenteredStr(int y, const char* str, const uint8_t* font) {
  u8g2.setFont(font);
  int16_t strWidth = u8g2.getStrWidth(str);
  int16_t x = (128 - strWidth) / 2;
  u8g2.drawStr(x, y, str);
}

//////////////////////   CONFIRM DIALOG   //////////////////////


void showConfirmDialog(const char* msg, bool& confirmed) {
  int sel = 0;  // 0=Yes, 1=No
  while (true) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 24, msg);
    u8g2.drawStr(0, 44, sel == 0 ? ">Yes" : " Yes");
    u8g2.drawStr(50, 44, sel == 1 ? ">No" : " No");
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn == BTN_UP || btn == BTN_DOWN) {
      sel = 1 - sel;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_MENU) {
      confirmed = (sel == 0);
      buttonClickSound();
      waitForButtonRelease();
      break;
    }
    delay(30);
  }
}


//////////////////////   MENU CONTROL   //////////////////////


void showMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);

  int start = menuIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > menuCount) end = menuCount;

  for (int i = start; i < end; i++) {
    int yPos = 24 + (i - start) * 14;  // vertical position

    if (i == menuIndex) {
      // Highlight background
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, yPos - 11, 128, 14);  // box aligned with text line

      // Invert text color
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);  // normal text
    }

    // Common text drawing (always runs)
    u8g2.drawStr(2, yPos, menuItems[i]);
  }

  u8g2.sendBuffer();
}

void handleMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {  // debounce
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      menuIndex--;
      if (menuIndex < 0) menuIndex = menuCount - 1;
      showMenu();
    } else if (btn == BTN_DOWN) {
      menuIndex++;
      if (menuIndex >= menuCount) menuIndex = 0;
      showMenu();
    } else if (btn == BTN_MENU) {
      // OK/Select
      if (menuIndex == 0) {
        // Back
        menuActive = false;
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        delay(200);
      } else if (menuIndex == 1) {
        // Clock Settings submenu
        menuActive = false;
        clockSettingsActive = true;
        clockSettingsIndex = 0;
        showClockSettingsMenu();
        delay(200);
      } else if (menuIndex == 2) {
        // Alarms submenu
        menuActive = false;
        alarmsActive = true;
        alarmsIndex = 0;
        showAlarmsMenu();
        delay(200);
      } else if (menuIndex == 3) {
        // Stopwatch submenu
        menuActive = false;
        stopwatchActive = true;
        stopwatchIndex = 0;
        showStopwatchMenu();
        delay(200);
      } else if (menuIndex == 4) {
        // Timer submenu
        menuActive = false;
        timerActive = true;
        timerIndex = 0;
        showTimerMenu();
        delay(200);
      } else if (menuIndex == 5) {
        // Weather submenu
        menuActive = false;
        weatherActive = true;
        weatherIndex = 0;
        showWeatherMenu();
        delay(200);
      } else if (menuIndex == 6) {
        // News submenu
        menuActive = false;
        newsActive = true;
        newsIndex = 0;
        showNewsMenu();
        delay(200);
      } else if (menuIndex == 7) {
        // Events submenu
        menuActive = false;
        eventsActive = true;
        eventsIndex = 0;
        showEventsMenu();
        delay(200);
      } else if (menuIndex == 8) {
        // Syatem Settings submenu
        menuActive = false;
        settingsActive = true;
        settingsIndex = 0;
        showSettingsMenu();
        delay(200);
      } else {
        // Show selected item (placeholder)
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(0, 24, "Selected:");
        u8g2.drawStr(0, 40, menuItems[menuIndex]);
        u8g2.sendBuffer();
        delay(1000);
        showMenu();
      }
    }
  }
}


//////////////////////   SUB-MENU CONTROL   //////////////////////

/*********** CLOCK SETTINGS ***********/

void showClockSettingsMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = clockSettingsIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > clockSettingsMenuCount) end = clockSettingsMenuCount;
  for (int i = start; i < end; i++) {
    if (i == clockSettingsIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, clockSettingsMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, clockSettingsMenu[i]);
    }
  }
  u8g2.sendBuffer();
}


//////////////////////   CLOCK SETTINGS HANDLERS   //////////////////////

void popupSetTimeDate() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    showMessage("Time Synced!", 24, 1200);
    waitForButtonRelease();
    return;
  }
  int hour = 12, min = 0, sec = 0, day = 1, mon = 1, year = 2024;
  int field = 0;  // 0=hour, 1=min, 2=sec, 3=day, 4=mon, 5=year
  const char* fields[] = { "Hour", "Minute", "Second", "Day", "Month", "Year" };
  bool done = false;
  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 20, fields[field]);
    char buf[16];
    sprintf(buf, "%02d", field == 0 ? hour : field == 1 ? min
                                           : field == 2 ? sec
                                           : field == 3 ? day
                                           : field == 4 ? mon
                                                        : year);
    u8g2.setFont(u8g2_font_t0_17_tr);
    u8g2.drawStr(0, 44, buf);
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn == BTN_UP) {
      if (field == 0) hour = (hour + 1) % 24;
      if (field == 1) min = (min + 1) % 60;
      if (field == 2) sec = (sec + 1) % 60;
      if (field == 3) day = (day < 31) ? day + 1 : 1;
      if (field == 4) mon = (mon < 12) ? mon + 1 : 1;
      if (field == 5) year++;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_DOWN) {
      if (field == 0) hour = (hour > 0) ? hour - 1 : 23;
      if (field == 1) min = (min > 0) ? min - 1 : 59;
      if (field == 2) sec = (sec > 0) ? sec - 1 : 59;
      if (field == 3) day = (day > 1) ? day - 1 : 31;
      if (field == 4) mon = (mon > 1) ? mon - 1 : 12;
      if (field == 5) year--;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_MENU) {
      field++;
      buttonClickSound();
      waitForButtonRelease();
      if (field > 5) {
        tm t;
        t.tm_hour = hour;
        t.tm_min = min;
        t.tm_sec = sec;
        t.tm_mday = day;
        t.tm_mon = mon - 1;
        t.tm_year = year - 1900;
        time_t tt = mktime(&t);
        struct timeval tv = { tt, 0 };
        settimeofday(&tv, nullptr);
        showMessage("Time Set!", 44, 1200);
        waitForButtonRelease();
        done = true;
      }
    }
    delay(30);
  }
}

void popupGMTOffset() {
  int offset = clockSettings.gmtOffset_sec;
  bool done = false;
  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 20, "GMT Offset");
    char buf[20];
    sprintf(buf, "%d s", offset);
    u8g2.setFont(u8g2_font_t0_17_tr);
    u8g2.drawStr(0, 44, buf);
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn == BTN_UP && offset < 43200) {
      offset += 900;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_DOWN && offset > -43200) {
      offset -= 900;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_RESET) {
      offset = 0;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_MENU) {
      clockSettings.gmtOffset_sec = offset;
      saveClockSettings();
      waitForButtonRelease();
      showMessage("Saved!", 44, 1200);
      done = true;
    }
    delay(30);
  }
}

void popupTimeFormat() {
  int format = clockSettings.timeFormat;
  bool done = false;
  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 20, "Time Format");
    char buf[16];
    sprintf(buf, "%d Hour", format);
    u8g2.setFont(u8g2_font_t0_17_tr);
    u8g2.drawStr(0, 44, buf);
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn == BTN_UP || btn == BTN_DOWN) {
      format = (format == 24) ? 12 : 24;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_MENU) {
      clockSettings.timeFormat = format;
      saveClockSettings();
      waitForButtonRelease();
      showMessage("Saved!", 44, 1200);
      done = true;
    }
    delay(30);
  }
}

void popupDateFormat() {
  const char* formats[] = {
    "dd.mm.yy", "dd.mm.yyyy", "dd.mmm.yy", "dd.mmm.yyyy",
    "mmm.dd.yy", "mmm.dd.yyyy", "dd.mm", "dd.mmm",
    "Day(Short) dd", "Day(Full) dd", "Day | dd mmm"
  };

  int idx = 0;
  int dateFormatCount = sizeof(formats) / sizeof(formats[0]);

  // Find current index from saved settings
  for (int i = 0; i < dateFormatCount; i++) {
    if (strcmp(clockSettings.dateFormat, formats[i]) == 0) {
      idx = i;
      break;
    }
  }

  bool done = false;
  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 20, "Date Format");
    u8g2.setFont(u8g2_font_t0_17_tr);
    u8g2.drawStr(0, 44, formats[idx]);
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn == BTN_UP) {
      idx = (idx + 1) % dateFormatCount;  // next item
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_DOWN) {
      idx = (idx - 1 + dateFormatCount) % dateFormatCount;  // previous item
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_MENU) {
      strcpy(clockSettings.dateFormat, formats[idx]);
      saveClockSettings();
      waitForButtonRelease();
      showMessage("Saved!", 44, 1200);
      done = true;
    }
    delay(30);
  }
}


void popupDayFormat() {
  const char* formats[] = { "Full", "Short" };
  int idx = strcmp(clockSettings.dayFormat, "Short") == 0 ? 1 : 0;
  bool done = false;
  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 20, "Day Format");
    u8g2.setFont(u8g2_font_t0_17_tr);
    u8g2.drawStr(0, 44, formats[idx]);
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn == BTN_UP || btn == BTN_DOWN) {
      idx = 1 - idx;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_MENU) {
      strcpy(clockSettings.dayFormat, formats[idx]);
      saveClockSettings();
      waitForButtonRelease();
      showMessage("Saved!", 44, 1200);
      done = true;
    }
    delay(30);
  }
}

void popupBoolOption(const char* label, bool& value) {
  bool tempValue = value;
  bool done = false;

  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 20, label);
    u8g2.setFont(u8g2_font_t0_17_tr);
    u8g2.drawStr(0, 44, tempValue ? "On" : "Off");
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn != BTN_NONE) {
      if (btn == BTN_UP || btn == BTN_DOWN) {
        tempValue = !tempValue;
        buttonClickSound();
        waitForButtonRelease();
      } else if (btn == BTN_MENU) {
        value = tempValue;
        saveClockSettings();
        buttonClickSound();
        waitForButtonRelease();
        showMessage("Saved!", 44, 1200);
        done = true;
      }
    }
    delay(50);
  }
}


//////////////////////   CLOCK SETTINGS MENU HANDLER   //////////////////////

void handleClockSettingsMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {  // debounce
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      clockSettingsIndex--;
      if (clockSettingsIndex < 0) clockSettingsIndex = clockSettingsMenuCount - 1;
      showClockSettingsMenu();
    } else if (btn == BTN_DOWN) {
      clockSettingsIndex++;
      if (clockSettingsIndex >= clockSettingsMenuCount) clockSettingsIndex = 0;
      showClockSettingsMenu();
    } else if (btn == BTN_MENU) {
      waitForButtonRelease();

      // Handle boolean options separately
      if (clockSettingsIndex >= 7 && clockSettingsIndex <= 9) {
        bool* targetValue = nullptr;
        const char* label = nullptr;

        switch (clockSettingsIndex) {
          case 7:
            targetValue = &clockSettings.weekNumber;
            label = "Week Number";
            break;
          case 8:
            targetValue = &clockSettings.totalDays;
            label = "Total Days";
            break;
          case 9:
            targetValue = &clockSettings.daylightSaving;
            label = "Daylight Saving";
            break;
        }

        if (targetValue != nullptr) {
          popupBoolOption(label, *targetValue);
          showClockSettingsMenu();
          return;
        }
      }
      // Handle other menu items
      switch (clockSettingsIndex) {
        case 0:  // Back
          clockSettingsActive = false;
          menuActive = true;
          showMenu();
          break;
        case 1:
          popupSetTimeDate();
          break;
        case 2:
          popupGMTOffset();
          break;
        case 3:
          configDateTime();
          break;
        case 4:
          popupTimeFormat();
          break;
        case 5:
          popupDateFormat();
          break;
        case 6:
          popupDayFormat();
          break;
      }
    }
  }
}

/*********** ALARM ***********/

void showAlarmsMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = alarmsIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > alarmsMenuCount) end = alarmsMenuCount;
  for (int i = start; i < end; i++) {
    if (i == alarmsIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, alarmsMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, alarmsMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

void popupAlarmTime(uint8_t& hour, uint8_t& minute) {
  int field = 0;  // 0=hour, 1=minute
  bool done = false;

  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 20, field == 0 ? "Hour" : "Minute");

    char buf[8];
    sprintf(buf, "%02d:%02d", hour, minute);
    u8g2.setFont(u8g2_font_t0_22_tr);
    u8g2.drawStr(30, 44, buf);

    // Highlight current field
    if (field == 0) {
      u8g2.drawBox(28, 46, 27, 2);
    } else {
      u8g2.drawBox(58, 46, 27, 2);
    }
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn != BTN_NONE) {
      buttonClickSound();
      waitForButtonRelease();

      if (btn == BTN_UP) {
        if (field == 0) {
          hour = (hour + 1) % 24;
        } else {
          minute = (minute + 1) % 60;
        }
      } else if (btn == BTN_DOWN) {
        if (field == 0) {
          hour = (hour > 0) ? hour - 1 : 23;
        } else {
          minute = (minute > 0) ? minute - 1 : 59;
        }
      } else if (btn == BTN_MENU) {
        if (field == 0) {
          field = 1;
        } else {
          done = true;
        }
      }
    }
    delay(50);
  }
}

// --- Tone Patterns ---
void tone_basicBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(50);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(500);
}

void tone_digital() {
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 80; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(300);  // higher pitch
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(300);
    }
    delay(150);
  }
}

void tone_classicBell() {
  for (int r = 0; r < 2; r++) {
    for (int i = 0; i < 150; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(800);  // lower pitch
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(800);
    }
    delay(200);
  }
}

void tone_chime() {
  for (int n = 0; n < 4; n++) {
    for (int i = 0; i < 120; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(600 - (n * 100));  // descending pitch
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(600 - (n * 100));
    }
    delay(100);
  }
}

void tone_morningBird() {
  for (int chirp = 0; chirp < 3; chirp++) {
    for (int i = 0; i < 50; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(200);  // sharp chirp
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(200);
    }
    delay(80);
    for (int i = 0; i < 70; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(500);
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(500);
    }
    delay(200);
  }
}

void tone_softBells() {
  for (int k = 0; k < 3; k++) {
    for (int i = 0; i < 100; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(1000);
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(1000);
    }
    delay(300);
  }
}

void tone_gentleRise() {
  int delayTime = 1000;
  for (int step = 0; step < 5; step++) {
    for (int i = 0; i < 80; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(delayTime);
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(delayTime);
    }
    delayTime -= 150;  // gradually higher pitch
    delay(150);
  }
}

void tone_marimba() {
  int freqs[] = { 800, 600, 400, 500 };
  for (int n = 0; n < 4; n++) {
    for (int i = 0; i < 100; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(freqs[n]);
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(freqs[n]);
    }
    delay(120);
  }
}

void playTonePreview(int toneIndex) {
  switch (toneIndex) {
    case 0: tone_basicBeep(); break;
    case 1: tone_digital(); break;
    case 2: tone_classicBell(); break;
    case 3: tone_chime(); break;
    case 4: tone_morningBird(); break;
    case 5: tone_softBells(); break;
    case 6: tone_gentleRise(); break;
    case 7: tone_marimba(); break;
  }
}

void popupAlarmTones() {
  int currentTone = alarmSettings.selectedTone;
  bool done = false;

  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 20, "Alarm Tone");
    u8g2.setFont(u8g2_font_t0_17_tr);
    u8g2.drawStr(0, 44, ALARM_TONES[currentTone]);
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn != BTN_NONE) {
      buttonClickSound();
      waitForButtonRelease();

      if (btn == BTN_UP) {
        currentTone = (currentTone + 1) % ALARM_TONES_COUNT;
        playTonePreview(currentTone);  // 🔊 preview
      } else if (btn == BTN_DOWN) {
        currentTone = (currentTone > 0) ? currentTone - 1 : ALARM_TONES_COUNT - 1;
        playTonePreview(currentTone);  // 🔊 preview
      } else if (btn == BTN_MENU) {
        alarmSettings.selectedTone = currentTone;
        saveAlarmSettings();
        showMessage("Saved!", 44, 1200);
        done = true;
      }
    }
    delay(50);
  }

  digitalWrite(BUZZER_PIN, LOW);  // stop buzzer when exiting
}



void popupSnoozeDuration() {
  int duration = alarmSettings.snoozeDuration;
  bool done = false;

  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 20, "Snooze Duration");
    char buf[16];
    sprintf(buf, "%d min", duration);
    u8g2.setFont(u8g2_font_t0_17_tr);
    u8g2.drawStr(0, 44, buf);
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn != BTN_NONE) {
      buttonClickSound();
      waitForButtonRelease();

      if (btn == BTN_UP && duration < 30) {
        duration++;
      } else if (btn == BTN_DOWN && duration > 1) {
        duration--;
      } else if (btn == BTN_MENU) {
        alarmSettings.snoozeDuration = duration;
        saveAlarmSettings();
        showMessage("Saved!", 44, 1200);
        done = true;
      }
    }
    delay(50);
  }
}

void handleAlarmEdit(int index) {
  const char* options[] = { "Edit", "Delete" };
  int selected = 0;
  bool done = false;

  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    char title[16];
    sprintf(title, "Alarm %d", index + 1);
    u8g2.drawStr(0, 12, title);

    char timeStr[8];
    sprintf(timeStr, "%02d:%02d", alarmSettings.alarms[index].hour,
            alarmSettings.alarms[index].minute);
    u8g2.drawStr(64, 12, timeStr);

    for (int i = 0; i < 2; i++) {
      if (i == selected) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(0, 13 + i * 14, 128, 14);
        u8g2.setDrawColor(0);
      }
      u8g2.drawStr(2, 24 + i * 14, options[i]);
      u8g2.setDrawColor(1);
    }
    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn != BTN_NONE) {
      buttonClickSound();
      waitForButtonRelease();

      if (btn == BTN_UP || btn == BTN_DOWN) {
        selected = 1 - selected;
      } else if (btn == BTN_MENU) {
        if (selected == 0) {  // Edit
          popupAlarmTime(alarmSettings.alarms[index].hour,
                         alarmSettings.alarms[index].minute);
          saveAlarmSettings();
          done = true;
        } else {  // Delete
          alarmSettings.alarms[index].active = false;
          saveAlarmSettings();
          showMessage("Deleted!", 44, 1200);
          done = true;
        }
      }
    }
    delay(50);
  }
}

void handleViewAlarms() {
  int selected = 0;
  bool done = false;

  while (!done) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 12, "View Alarms");

    int visibleCount = 0;
    const int rowHeight = 12;  // better fit on 64px screen
    const int startY = 24;

    for (int i = 0; i < 3; i++) {
      if (alarmSettings.alarms[i].active) {
        char buf[20];
        sprintf(buf, "Alarm %d  %02d:%02d", i + 1,
                alarmSettings.alarms[i].hour,
                alarmSettings.alarms[i].minute);

        int y = startY + visibleCount * rowHeight;
        if (visibleCount == selected) {
          u8g2.setDrawColor(1);
          u8g2.drawBox(0, y - 10, 128, rowHeight);
          u8g2.setDrawColor(0);
        }
        u8g2.drawStr(2, y, buf);
        u8g2.setDrawColor(1);
        visibleCount++;
      }
    }

    // Always show "Back" option
    int backIndex = visibleCount;
    int backY = startY + visibleCount * rowHeight;

    if (selected == backIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, backY - 10, 128, rowHeight);
      u8g2.setDrawColor(0);
    }
    u8g2.drawStr(2, backY, "Back");
    u8g2.setDrawColor(1);

    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn != BTN_NONE) {
      buttonClickSound();
      waitForButtonRelease();

      if (btn == BTN_UP) {
        selected = (selected > 0) ? selected - 1 : backIndex;
      } else if (btn == BTN_DOWN) {
        selected = (selected < backIndex) ? selected + 1 : 0;
      } else if (btn == BTN_MENU) {
        if (selected == backIndex) {
          done = true;  // Back pressed
        } else {
          // Map selection to actual alarm index
          int alarmIndex = -1;
          int count = 0;
          for (int i = 0; i < 3; i++) {
            if (alarmSettings.alarms[i].active) {
              if (count == selected) {
                alarmIndex = i;
                break;
              }
              count++;
            }
          }
          if (alarmIndex != -1) {
            handleAlarmEdit(alarmIndex);
          }
        }
      }
    }
    delay(50);
  }
}

void checkAlarms() {
  if (alarmTriggered) return;  // Already handling an alarm

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  int nowHour = timeinfo.tm_hour;
  int nowMinute = timeinfo.tm_min;

  // --- Normal alarms ---
  for (int i = 0; i < 3; i++) {
    if (alarmSettings.alarms[i].active) {
      if (alarmSettings.alarms[i].hour == nowHour && alarmSettings.alarms[i].minute == nowMinute) {
        alarmTriggered = true;
        triggeredAlarmIndex = i;
        alarmBlinkLast = millis();
        alarmBlinkState = false;
        break;
      }
    }
  }
}


void playAlarmTone(int toneIndex) {
  for (int repeat = 0; repeat < 3; repeat++) {
    playTonePreview(toneIndex);
    delay(100);
  }
}


// --- Alarm UI ---
void handleAlarmTriggerUI() {
  if (triggeredAlarmIndex < 0 || triggeredAlarmIndex > 2) return;

  unsigned long now = millis();
  // Blink display every 400ms
  if (now - alarmBlinkLast > 400) {
    alarmBlinkState = !alarmBlinkState;
    alarmBlinkLast = now;
  }

  // Show blinking alarm time
  u8g2.clearBuffer();
  drawCenteredStr(16, "!!! ALARM !!!", u8g2_font_t0_11_tr);

  uint8_t h = alarmSettings.alarms[triggeredAlarmIndex].hour;
  uint8_t m = alarmSettings.alarms[triggeredAlarmIndex].minute;

  if (alarmBlinkState) {
    char timeStr[8];
    sprintf(timeStr, "%02d:%02d", h, m);
    drawCenteredStr(44, timeStr, u8g2_font_logisoso22_tr);
  }

  drawCenteredStr(62, "OK:Stop", u8g2_font_t0_11_tr);

  u8g2.sendBuffer();

  // Play alarm tone (non-blocking, short burst)
  playTonePreview(alarmSettings.alarms[triggeredAlarmIndex].tone);

  // Button handling
  ButtonType btn = readButton();
  if (btn == BTN_MENU) {
    // Stop and delete alarm
    alarmSettings.alarms[triggeredAlarmIndex].active = false;
    saveAlarmSettings();
    alarmTriggered = false;
    triggeredAlarmIndex = -1;
    digitalWrite(BUZZER_PIN, LOW);
    showMessage("Alarm Stopped!", 44, 1200);
    waitForButtonRelease();
    delay(200);
    u8g2.clearBuffer();
  }
}

void handleAddAlarm() {
  // Find first inactive alarm slot
  int slot = -1;
  for (int i = 0; i < 3; i++) {
    if (!alarmSettings.alarms[i].active) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    showMessage("Max alarms reached!", 44, 1200);
    return;
  }

  // Headline: Alarm X
  char title[16];
  sprintf(title, "Alarm %d", slot + 1);

  // Initialize new alarm
  alarmSettings.alarms[slot].hour = 7;
  alarmSettings.alarms[slot].minute = 0;
  alarmSettings.alarms[slot].tone = alarmSettings.selectedTone;
  alarmSettings.alarms[slot].snooze = true;

  // Edit time
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(0, 12, title);
  u8g2.sendBuffer();

  popupAlarmTime(alarmSettings.alarms[slot].hour,
                 alarmSettings.alarms[slot].minute);

  alarmSettings.alarms[slot].active = true;
  saveAlarmSettings();
  showMessage("Alarm Saved!", 44, 1200);
}

void handleAlarmsMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();

  if (btn != BTN_NONE && now - lastPress > 250) {
    lastPress = now;
    buttonClickSound();

    if (btn == BTN_UP) {
      alarmsIndex--;
      if (alarmsIndex < 0) alarmsIndex = alarmsMenuCount - 1;
      showAlarmsMenu();
    } else if (btn == BTN_DOWN) {
      alarmsIndex++;
      if (alarmsIndex >= alarmsMenuCount) alarmsIndex = 0;
      showAlarmsMenu();
    } else if (btn == BTN_MENU) {
      waitForButtonRelease();

      switch (alarmsIndex) {
        case 0:  // Back
          alarmsActive = false;
          menuActive = true;
          showMenu();
          break;
        case 1:  // Add New Alarm
          handleAddAlarm();
          showAlarmsMenu();
          break;
        case 2:  // View Alarms
          handleViewAlarms();
          showAlarmsMenu();
          break;
        case 3:  // Alarm Tones
          popupAlarmTones();
          showAlarmsMenu();
          break;
        case 4:  // Snooze Duration
          popupSnoozeDuration();
          showAlarmsMenu();
          break;
      }
    }
  }
}

/*********** STOPWATCH ***********/

void showStopwatchMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = stopwatchIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > stopwatchMenuCount) end = stopwatchMenuCount;
  for (int i = start; i < end; i++) {
    if (i == stopwatchIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, stopwatchMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, stopwatchMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

void resetStopwatch() {
  stopwatchStart = 0;
  stopwatchElapsed = 0;
  stopwatchRunning = false;
  lastLapTime = 0;
  lap1 = 0;
  lap2 = 0;
}

String formatTime(unsigned long ms) {
  unsigned long totalSeconds = ms / 1000;
  unsigned int hours = totalSeconds / 3600;
  unsigned int minutes = (totalSeconds % 3600) / 60;
  unsigned int seconds = totalSeconds % 60;

  char buffer[12];
  sprintf(buffer, "%02u:%02u:%02u", hours, minutes, seconds);
  return String(buffer);
}

void drawStopwatchUI(int cursor = 0) {
  u8g2.clearBuffer();

  // Title
  // u8g2.setFont(u8g2_font_t0_11_tr);
  // u8g2.drawStr(0, 12, "Stopwatch");

  drawCenteredStr(9, "Stopwatch", u8g2_font_t0_11_tr);

  // Back option bottom right
  if (cursor == -1) {
    u8g2.drawBox(90, 54, 38, 12);
    u8g2.setDrawColor(0);
    u8g2.drawStr(92, 63, "<Back");
    u8g2.setDrawColor(1);
  } else {
    u8g2.drawStr(92, 63, "<Back");
  }

  // Stopwatch main time (80px area)
  unsigned long elapsed = stopwatchElapsed;
  if (stopwatchRunning) elapsed += millis() - stopwatchStart;

  String timeStr = formatTime(elapsed);

  // u8g2.setFont(u8g2_font_logisoso22_tr);
  // u8g2.drawStr(2, 38, timeStr.c_str());

  drawCenteredStr(38, timeStr.c_str(), u8g2_font_logisoso22_tr);

  // Show only L1 and L2
  u8g2.setFont(u8g2_font_t0_11_tr);
  if (lap1 > 0) {
    String l1Str = "L1 " + formatTime(lap1);
    u8g2.drawStr(2, 52, l1Str.c_str());
  }
  if (lap2 > 0) {
    String l2Str = "L2 " + formatTime(lap2);
    u8g2.drawStr(2, 64, l2Str.c_str());
  }

  u8g2.sendBuffer();
}

void handleStopwatchUI() {
  waitForButtonRelease();  // 🔧 debounce fix
  static unsigned long lastPress = 0;
  static int cursor = 0;  // -1 = back, 0 = stopwatch
  unsigned long now = millis();

  drawStopwatchUI(cursor);

  ButtonType btn = readButton();
  if (btn != BTN_NONE && now - lastPress > 250) {
    lastPress = now;
    buttonClickSound();
    waitForButtonRelease();

    if (cursor == -1) {
      if (btn == BTN_MENU) {
        resetStopwatch();
        if (shortcutStopwatchActive) {
          shortcutStopwatchActive = false;  // Go back to main UI
        } else {
          stopwatchActive = false;
          menuActive = true;
        }
        return;
      }
      if (btn == BTN_UP || btn == BTN_DOWN) cursor = 0;
    } else {
      if (btn == BTN_MENU) {  // Play/Pause
        if (!stopwatchRunning) {
          stopwatchStart = millis();
          stopwatchRunning = true;
        } else {
          stopwatchElapsed += millis() - stopwatchStart;
          stopwatchRunning = false;
        }
      } else if (btn == BTN_UP) {
        if (stopwatchRunning) {
          unsigned long elapsed = stopwatchElapsed + millis() - stopwatchStart;
          unsigned long lapInterval = elapsed - lastLapTime;
          lastLapTime = elapsed;

          // Shift laps
          lap2 = lap1;
          lap1 = lapInterval;
        }
      } else if (!stopwatchRunning && btn == BTN_RESET) {
        resetStopwatch();
      } else if (btn == BTN_DOWN) {
        cursor = -1;  // jump to back
      }
    }
  }
}

void handleShortcutStopwatchUI() {
  while (true) {
    handleStopwatchUI();
    // If user presses "Back" (cursor == -1 and BTN_MENU), exit to main UI
    if (!shortcutStopwatchActive) break;
    delay(10);
  }
  shortcutStopwatchActive = false;
}

void handleStopwatchMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();

  if (btn != BTN_NONE && now - lastPress > 250) {
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      stopwatchIndex--;
      if (stopwatchIndex < 0) stopwatchIndex = stopwatchMenuCount - 1;
      showStopwatchMenu();
    } else if (btn == BTN_DOWN) {
      stopwatchIndex++;
      if (stopwatchIndex >= stopwatchMenuCount) stopwatchIndex = 0;
      showStopwatchMenu();
    } else if (btn == BTN_MENU) {
      if (stopwatchIndex == 0) {
        // Back to main menu
        stopwatchActive = false;
        menuActive = true;
        showMenu();
        // delay(200);
      } else if (stopwatchIndex == 1) {
        // Open Stopwatch UI
        while (stopwatchActive) {
          if (!stopwatchActive) break;
          handleStopwatchUI();
          delay(20);
        }
      }
    }
  }
}


/*********** TIMER ***********/

void showTimerMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = timerIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > timerMenuCount) end = timerMenuCount;
  for (int i = start; i < end; i++) {
    if (i == timerIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, timerMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, timerMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

// --- Timer pop-up for setting duration ---
void popupSetTimerDuration() {
  int hour = timerSetMillis / 3600000;
  int minute = (timerSetMillis % 3600000) / 60000;
  int second = (timerSetMillis % 60000) / 1000;
  int field = 0;  // 0=hour, 1=minute, 2=second
  const char* fields[] = { "Hour", "Minute", "Second" };
  bool done = false;

  while (!done) {
    u8g2.clearBuffer();

    // Title
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 12, "Set Timer");

    // Current field label
    u8g2.drawStr(0, 28, fields[field]);

    // Timer values
    char buf[16];
    sprintf(buf, "%02d:%02d:%02d", hour, minute, second);
    u8g2.setFont(u8g2_font_logisoso22_tr);  // same as stopwatch
    u8g2.drawStr(10, 55, buf);

    u8g2.sendBuffer();

    ButtonType btn = readButton();
    if (btn == BTN_UP) {
      if (field == 0 && hour < 23) hour++;
      if (field == 1 && minute < 59) minute++;
      if (field == 2 && second < 59) second++;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_DOWN) {
      if (field == 0 && hour > 0) hour--;
      if (field == 1 && minute > 0) minute--;
      if (field == 2 && second > 0) second--;
      buttonClickSound();
      waitForButtonRelease();
    } else if (btn == BTN_MENU) {
      field++;
      buttonClickSound();
      waitForButtonRelease();
      if (field > 2) {
        timerSettings.hours = hour;
        timerSettings.minutes = minute;
        timerSettings.seconds = second;

        saveTimerSettings();

        timerSetMillis = (hour * 3600000UL) + (minute * 60000UL) + (second * 1000UL);
        timerRemainingMillis = timerSetMillis;

        showMessage("Saved!", 44, 1000);
        done = true;
      }
    }
    delay(30);
  }
}

// --- Timer UI ---
void drawTimerUI(bool flash = false) {
  u8g2.clearBuffer();
  // u8g2.setFont(u8g2_font_t0_11_tr);
  // u8g2.drawStr(0, 12, "Timer");
  drawCenteredStr(9, "Timer", u8g2_font_t0_11_tr);

  // Timer value
  unsigned long ms = timerRemainingMillis;
  unsigned int hour = ms / 3600000;
  unsigned int minute = (ms % 3600000) / 60000;
  unsigned int second = (ms % 60000) / 1000;

  char timeStr[16];
  sprintf(timeStr, "%02d:%02d:%02d", hour, minute, second);

  // u8g2.setFont(u8g2_font_logisoso22_tr);

  // Flash when time up
  if (!(flash && timerTimeUp)) {
    // u8g2.drawStr(10, 45, timeStr);
    drawCenteredStr(45, timeStr, u8g2_font_logisoso22_tr);
  }

  u8g2.sendBuffer();
}


// --- Timer handler ---
void handleTimerUI() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastPress = 0;
  static unsigned long lastFlash = 0;
  bool exitUI = false;
  bool flashState = false;

  while (!exitUI && (timerActive || shortcutTimerActive)) {
    unsigned long now = millis();

    // Timer countdown
    if (timerRunning && timerRemainingMillis > 0 && now - lastUpdate >= 100) {
      unsigned long elapsed = now - lastUpdate;
      if (elapsed > timerRemainingMillis) elapsed = timerRemainingMillis;
      timerRemainingMillis -= elapsed;
      lastUpdate = now;

      if (timerRemainingMillis == 0) {
        timerRunning = false;
        timerTimeUp = true;
        lastFlash = now;
        digitalWrite(BUZZER_PIN, HIGH);  // start beep
      }
    }

    // Handle flashing + beep
    if (timerTimeUp) {
      if (now - lastFlash >= 500) {
        flashState = !flashState;
        lastFlash = now;
        digitalWrite(BUZZER_PIN, flashState ? HIGH : LOW);
      }
      drawTimerUI(flashState);
    } else {
      drawTimerUI();
    }

    // Button handling
    ButtonType btn = readButton();
    if (btn != BTN_NONE && now - lastPress > 250) {
      lastPress = now;
      buttonClickSound();
      waitForButtonRelease();

      if (timerTimeUp && btn == BTN_MENU) {
        // Stop flashing & beep
        timerTimeUp = false;
        digitalWrite(BUZZER_PIN, LOW);
        timerRemainingMillis = timerSetMillis;  // reset
      } else if (btn == BTN_MENU) {
        if (!timerRunning && timerRemainingMillis > 0) {
          timerRunning = true;
          lastUpdate = millis();
        } else if (timerRunning) {
          timerRunning = false;
        }
      } else if (!timerRunning && btn == BTN_UP) {
        // Reset timer value to default from EEPROM
        loadTimerSettings();
        timerSetMillis = (timerSettings.hours * 3600000UL) + (timerSettings.minutes * 60000UL) + (timerSettings.seconds * 1000UL);
        timerRemainingMillis = timerSetMillis;
        showMessage("Timer Reset!", 44, 800);
      } else if (!timerRunning && btn == BTN_DOWN) {
        if (shortcutTimerActive) {
          // If shortcut, DOWN = exit to home UI
          shortcutTimerActive = false;
          return;
        } else {
          // If from menu, DOWN = go back one step (to menu)
          exitUI = true;
          return;
        }
      }
    }
    delay(20);
  }
  if (shortcutTimerActive) {
    shortcutTimerActive = false;
  } else {
    timerActive = false;
  }
}

void handleShortcutTimerUI() {
  while (true) {
    handleTimerUI();
    // If user presses "Back" (exitUI in handleTimerUI), exit to main UI
    if (!shortcutTimerActive) break;
    delay(10);
  }
  shortcutTimerActive = false;
}

// --- Timer menu handler ---
void handleTimerMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {  // debounce
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      timerIndex--;
      if (timerIndex < 0) timerIndex = timerMenuCount - 1;
      showTimerMenu();
    } else if (btn == BTN_DOWN) {
      timerIndex++;
      if (timerIndex >= timerMenuCount) timerIndex = 0;
      showTimerMenu();
    } else if (btn == BTN_MENU) {
      waitForButtonRelease();
      switch (timerIndex) {
        case 0:  // Back
          timerActive = false;
          menuActive = true;
          timerIndex = 0;
          showMenu();
          break;
        case 1:  // Set Duration
          // timerActive = false;
          // menuActive = true;
          popupSetTimerDuration();
          showTimerMenu();
          break;
        case 2:  // Start/Pause
          handleTimerUI();
          showTimerMenu();
          break;
      }
    }
  }
}

/*********** WEATHER ***********/

void showWeatherMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = weatherIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > weatherMenuCount) end = weatherMenuCount;
  for (int i = start; i < end; i++) {
    if (i == weatherIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, weatherMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, weatherMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

void handleWeatherMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {  // debounce
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      weatherIndex--;
      if (weatherIndex < 0) weatherIndex = weatherMenuCount - 1;
      showWeatherMenu();
    } else if (btn == BTN_DOWN) {
      weatherIndex++;
      if (weatherIndex >= weatherMenuCount) weatherIndex = 0;
      showWeatherMenu();
    } else if (btn == BTN_MENU) {
      // OK/Select
      if (weatherIndex == 0) {
        // Back to main menu
        weatherActive = false;
        menuActive = true;
        showMenu();
        delay(200);
      } else {
        // Placeholder for submenu actions
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(0, 24, "Selected:");
        u8g2.drawStr(0, 40, weatherMenu[weatherIndex]);
        u8g2.sendBuffer();
        delay(1000);
        showWeatherMenu();
      }
    }
  }
}

/*********** NEWS ***********/

void showNewsMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = newsIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > newsMenuCount) end = newsMenuCount;
  for (int i = start; i < end; i++) {
    if (i == newsIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, newsMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, newsMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

void handleNewsMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {  // debounce
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      newsIndex--;
      if (newsIndex < 0) newsIndex = newsMenuCount - 1;
      showNewsMenu();
    } else if (btn == BTN_DOWN) {
      newsIndex++;
      if (newsIndex >= newsMenuCount) newsIndex = 0;
      showNewsMenu();
    } else if (btn == BTN_MENU) {
      // OK/Select
      if (newsIndex == 0) {
        // Back to main menu
        newsActive = false;
        menuActive = true;
        showMenu();
        delay(200);
      } else {
        // Placeholder for submenu actions
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(0, 24, "Selected:");
        u8g2.drawStr(0, 40, newsMenu[newsIndex]);
        u8g2.sendBuffer();
        delay(1000);
        showNewsMenu();
      }
    }
  }
}

/*********** EVENTS ***********/

void showEventsMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = eventsIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > eventsMenuCount) end = eventsMenuCount;
  for (int i = start; i < end; i++) {
    if (i == eventsIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, eventsMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, eventsMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

// -- Load Events

struct EventItem {
  String date;  // "YYYY-MM-DD"
  String text;
};
#define MAX_EVENTS 256
EventItem eventsList[MAX_EVENTS];
int eventsCount = 0;

bool loadEventsFromJson(const char* filename) {
  eventsCount = 0;
  File file = SPIFFS.open(filename, "r");
  if (!file) return false;

  size_t size = file.size();
  if (size == 0) return false;

  DynamicJsonDocument doc(12288);  // give some breathing room

  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    Serial.print(F("JSON parse failed: "));
    Serial.println(err.c_str());
    return false;
  } else {
    Serial.println("JSON parse OK!");
  }

  for (JsonPair kv : doc.as<JsonObject>()) {
    String dateKey = kv.key().c_str();  // "MM-DD"
    JsonArray arr = kv.value().as<JsonArray>();
    for (JsonVariant v : arr) {
      if (eventsCount < MAX_EVENTS) {
        eventsList[eventsCount].date = dateKey;
        eventsList[eventsCount].text = v.as<String>();
        eventsCount++;
      }
    }
  }
  return true;
}



// -- Scrolling Sentences
const int SCROLL_SPEED = 30;  // ms per pixel

void drawScrollingText(const char* text, int x, int y, int width, const uint8_t* font, int gap = 20) {
  static unsigned long lastScrollTime = 0;
  static int scrollOffset = 0;
  static String lastText = "";

  u8g2.setFont(font);
  int textWidth = u8g2.getStrWidth(text);
  int ascent = u8g2.getAscent();
  int descent = u8g2.getDescent();

  // ✅ Reset scroll if new text
  if (lastText != text) {
    lastText = text;
    scrollOffset = 0;
    lastScrollTime = millis();
  }

  // If text fits inside width → no scrolling
  if (textWidth <= width) {
    u8g2.setClipWindow(x, y - ascent, x + width, y - descent);
    u8g2.drawStr(x, y, text);
    u8g2.setMaxClipWindow();
    return;
  }

  unsigned long now = millis();
  if (now - lastScrollTime > SCROLL_SPEED) {
    scrollOffset++;
    if (scrollOffset > textWidth + gap) scrollOffset = 0;
    lastScrollTime = now;
  }

  u8g2.setClipWindow(x, y - ascent - 1, x + width, y - descent + 1);

  // First segment
  u8g2.drawStr(x - scrollOffset, y, text);

  // Second segment for seamless looping
  u8g2.drawStr(x - scrollOffset + textWidth + gap, y, text);

  u8g2.setMaxClipWindow();
}

void drawScrollingText2(const char* text, int x, int y, int width, const uint8_t* font, int gap = 20) {
  static unsigned long lastScrollTime = 0;
  static int scrollOffset = 0;
  static String lastText = "";

  u8g2.setFont(font);
  int textWidth = u8g2.getStrWidth(text);
  int ascent = u8g2.getAscent();
  int descent = u8g2.getDescent();

  // ✅ Reset scroll if new text
  if (lastText != text) {
    lastText = text;
    scrollOffset = 0;
    lastScrollTime = millis();
  }

  // If text fits inside width → no scrolling
  if (textWidth <= width) {
    u8g2.setClipWindow(x, y - ascent, x + width, y - descent);
    u8g2.drawStr(x, y, text);
    u8g2.setMaxClipWindow();
    return;
  }

  unsigned long now = millis();
  if (now - lastScrollTime > SCROLL_SPEED) {
    scrollOffset++;
    if (scrollOffset > textWidth + gap) scrollOffset = 0;
    lastScrollTime = now;
  }

  u8g2.setClipWindow(x, y - ascent - 1, x + width, y - descent + 1);

  // First segment
  u8g2.drawStr(x - scrollOffset, y, text);

  // Second segment for seamless looping
  u8g2.drawStr(x - scrollOffset + textWidth + gap, y, text);

  u8g2.setMaxClipWindow();
}


// -- Show Events

void showEventsList(bool todayOnly) {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  char todayStr[6];
  strftime(todayStr, sizeof(todayStr), "%m-%d", &timeinfo);

  int selected = 0;
  int visibleStart = 0;
  const int visibleCount = 3;
  unsigned long lastPress = 0;

  // Build filtered index list
  int filtered[MAX_EVENTS];
  int filteredCount = 0;
  for (int i = 0; i < eventsCount; i++) {
    if (todayOnly && eventsList[i].date != String(todayStr)) continue;
    filtered[filteredCount++] = i;
  }

  while (true) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);

    // Header
    u8g2.drawStr(0, 12, todayOnly ? "Today Events" : "All Events");

    // Show selected event’s date
    if (filteredCount > 0) {
      int eventIndex = filtered[selected];
      String dateStr = eventsList[eventIndex].date;
      int16_t w = u8g2.getStrWidth(dateStr.c_str());
      u8g2.drawStr(128 - w, 12, dateStr.c_str());
    }

    // Show events
    for (int j = 0; j < visibleCount; j++) {
      int idx = visibleStart + j;
      if (idx >= filteredCount) break;
      int i = filtered[idx];
      int y = 28 + j * 14;

      if (idx == selected) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(0, y - 10, 128, 14);
        u8g2.setDrawColor(0);

        // ✅ Always draw with scrolling function
        drawScrollingText(eventsList[i].text.c_str(), 2, y, 124, u8g2_font_t0_11_tr, 30);

        u8g2.setDrawColor(1);
      } else {
        // Truncate non-selected lines
        String line = eventsList[i].text;
        if (line.length() > 18) line = line.substring(0, 18);
        u8g2.drawStr(2, y, line.c_str());
      }
    }

    if (filteredCount == 0) {
      u8g2.drawStr(2, 36, "No events found.");
    }

    u8g2.sendBuffer();

    // Handle buttons
    ButtonType btn = readButton();
    unsigned long now = millis();
    if (btn != BTN_NONE && now - lastPress > 200) {
      lastPress = now;
      buttonClickSound();

      if (btn == BTN_UP && selected > 0) {
        selected--;
        if (selected < visibleStart) visibleStart = selected;
      } else if (btn == BTN_DOWN && selected < filteredCount - 1) {
        selected++;
        if (selected > visibleStart + visibleCount - 1)
          visibleStart = selected - (visibleCount - 1);
      } else if (btn == BTN_MENU) {
        waitForButtonRelease();
        break;  // exit
      }
    }
  }
}


// -- Monthly Calender
void showMonthCalendar(int month, int year) {
  unsigned long lastPress = 0;
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int todayDay = timeinfo.tm_mday;
  int todayMonth = timeinfo.tm_mon + 1;
  int todayYear = timeinfo.tm_year + 1900;
  while (true) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);  // ✅ Compact font

    // --- Title ---
    char title[20];
    sprintf(title, "%04d-%02d", year, month);
    u8g2.drawStr((128 - u8g2.getStrWidth(title)) / 2, 6, title);

    // --- Weekdays header ---
    const char* days[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
    for (int i = 0; i < 7; i++) {
      u8g2.drawStr(4 + i * 18, 14, days[i]);
    }

    // --- Find first day of month ---
    tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = 1;
    mktime(&t);
    int firstDay = t.tm_wday;

    // --- Days in month ---
    int daysInMonth = 31;
    if (month == 2) daysInMonth = (year % 4 == 0) ? 29 : 28;
    else if (month == 4 || month == 6 || month == 9 || month == 11) daysInMonth = 30;

    // --- Draw days grid (6 rows fit in 64px) ---
    int row = 0, col = firstDay;
    for (int d = 1; d <= daysInMonth; d++) {
      char buf[3];
      sprintf(buf, "%2d", d);
      // ✅ Highlight today
      if (d == todayDay && month == todayMonth && year == todayYear) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(4 + col * 18, 22 + row * 8 - 7, 12, 8);
        u8g2.setDrawColor(0);
      }
      u8g2.drawStr(4 + col * 18, 22 + row * 8, buf);
      u8g2.setDrawColor(1);  // Reset for next item

      col++;
      if (col > 6) {
        col = 0;
        row++;
      }
    }

    u8g2.sendBuffer();

    // --- Debounced button read ---
    ButtonType btn = readButton();
    unsigned long now = millis();
    if (btn != BTN_NONE && now - lastPress > 200) {
      lastPress = now;
      buttonClickSound();  // ✅ buzzer feedback

      if (btn == BTN_UP) {
        month++;
        if (month > 12) {
          month = 1;
          year++;
        }
      } else if (btn == BTN_DOWN) {
        month--;
        if (month < 1) {
          month = 12;
          year--;
        }
      } else if (btn == BTN_MENU) {
        waitForButtonRelease();
        break;
      }
    }
  }
}



// -- Events Handle

void handleEventsMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      eventsIndex--;
      if (eventsIndex < 0) eventsIndex = eventsMenuCount - 1;
      showEventsMenu();
    } else if (btn == BTN_DOWN) {
      eventsIndex++;
      if (eventsIndex >= eventsMenuCount) eventsIndex = 0;
      showEventsMenu();
    } else if (btn == BTN_MENU) {
      waitForButtonRelease();
      switch (eventsIndex) {
        case 0:  // Back
          eventsActive = false;
          menuActive = true;
          showMenu();
          break;
        case 1:  // Today's Events
          if (loadEventsFromJson("/events.json")) {
            showEventsList(true);
          } else {
            showMessage("No events file!", 44, 1200);
          }
          showEventsMenu();
          break;
        case 2:  // All Events
          if (loadEventsFromJson("/events.json")) {
            showEventsList(false);
          } else {
            showMessage("No events file!", 44, 1200);
          }
          showEventsMenu();
          break;
        case 3:
          {  // View Month Calc.
            struct tm timeinfo;
            getLocalTime(&timeinfo);
            int month = timeinfo.tm_mon + 1;
            int year = timeinfo.tm_year + 1900;
            showMonthCalendar(month, year);
            showEventsMenu();
            break;
          }
      }
    }
  }
}

/*********** SYSTEM SETTINGS ***********/

void showSettingsMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = settingsIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > settingsMenuCount) end = settingsMenuCount;
  for (int i = start; i < end; i++) {
    if (i == settingsIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, settingsMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, settingsMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

// --- WiFi Setup ---

void showWiFiMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = wifiIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > wifiMenuCount) end = wifiMenuCount;
  for (int i = start; i < end; i++) {
    if (i == wifiIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, wifiMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, wifiMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

// --- WiFi Settings Handler ---
void handleWiFiMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      wifiIndex--;
      if (wifiIndex < 0) wifiIndex = wifiMenuCount - 1;
      showWiFiMenu();
    } else if (btn == BTN_DOWN) {
      wifiIndex++;
      if (wifiIndex >= wifiMenuCount) wifiIndex = 0;
      showWiFiMenu();
    } else if (btn == BTN_MENU) {
      waitForButtonRelease();
      switch (wifiIndex) {
        case 0:  // Back
          wifiActive = false;
          settingsActive = true;
          showSettingsMenu();
          break;
        case 1:  // Connect WiFi
          connectToSavedWiFi();
          showWiFiMenu();
          break;
        case 2:  // WiFi Status
          checkAndConnectWiFi();
          showWiFiMenu();
          break;
        case 3:  // Disconnect
          WiFi.disconnect();
          showMessage("WiFi Disconnected!", 24, 1200);
          showWiFiMenu();
          break;
        case 4:
          {  // Forget WiFi
            bool confirmed = false;
            showConfirmDialog("Forget WiFi?", confirmed);
            if (confirmed) {
              WiFiManager wm;
              wm.resetSettings();
              showMessage("WiFi Reset Done!", 24, 1200);
              ESP.restart();
            } else {
              showMessage("Cancelled!", 24, 1200);
              showWiFiMenu();
            }
            break;
          }
      }
    }
  }
}

// -- Display Settings --

void showThemePreview(int theme) {
  switch (theme) {
    case 0:  // Theme 1: themeClassicTimeDateEvents
      themeClassicTimeDateEvents();
      break;
    case 1:  // Theme 2: Minimal
      themeMinimal();
      break;
    case 2:  // Theme 3: Classic 2
      themeClassic2();
      break;
    case 3:  // Theme 4: Minimal Inverted
      themeMinimalInverted();
      break;
    case 4:  // Theme 5: Classic Inverted
      themeClassicDTEInverted();
      break;
    case 5:  // Theme 6: Analog Dial
      themeAnalogClock();
      break;
    case 6:  // Theme 7: Detailed Information
      themeDetailedInformations();
      break;
    case 7:
      themeBoxee();  // Theme 8: boxee
      break;
    case 8:
      themeClassicBoxee();  // theme 9: Classic Boxee
      break;
    case 9:
      themeDial();  // theme 10: Dial
      break;
    case 10:
      themeBarClock();  // theme 11: Bar Clock
      break;
    case 11:
      themeClassic3();  // theme 12: Classic 3.0
      break;
    case 12:
      themeWeatherView();  // theme 13: Weather View
      break;
    default:
      themeClassicTimeDateEvents();
      break;
  }
}

// --- Theme 1:  themeClassicTimeDateEvents ---
const char* mapDateFormat(const char* fmt) {
  if (strcmp(fmt, "dd.mm.yy") == 0) return "%d.%m.%y";
  if (strcmp(fmt, "dd.mm.yyyy") == 0) return "%d.%m.%Y";
  if (strcmp(fmt, "dd.mmm.yy") == 0) return "%d.%b.%y";
  if (strcmp(fmt, "dd.mmm.yyyy") == 0) return "%d.%b.%Y";
  if (strcmp(fmt, "mmm.dd.yy") == 0) return "%b.%d.%y";
  if (strcmp(fmt, "mmm.dd.yyyy") == 0) return "%b.%d.%Y";
  if (strcmp(fmt, "dd.mm") == 0) return "%d.%m";
  if (strcmp(fmt, "dd.mmm") == 0) return "%d.%b";
  if (strcmp(fmt, "Day(Short) dd") == 0) return "%a %d";
  if (strcmp(fmt, "Day(Full) dd") == 0) return "%A %d";
  if (strcmp(fmt, "Day | dd mmm") == 0) return "%a | %d %b";
  return "%d.%m.%Y";  // default
}

void themeClassicTimeDateEvents() {
  u8g2.clearBuffer();

  // Date & Time
  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  char dateStr[16] = "--/--/----";  // Date
  char timeStr[16] = "--:--:--";    // Time
  char todayStr[6];                 // "MM-DD"

  if (hasTime) {
    strftime(todayStr, sizeof(todayStr), "%m-%d", &timeinfo);  // For events
    if (clockSettings.timeFormat == 12) {
      strftime(timeStr, sizeof(timeStr), "%I:%M:%S", &timeinfo);  // 12h format
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);  // 24h format
    }
  }

  // --- Events ---
  String allEvents = "";
  for (int i = 0; i < eventsCount; i++) {
    if (eventsList[i].date == String(todayStr)) {
      if (allEvents.length() > 0) allEvents += " | ";
      allEvents += eventsList[i].text;
    }
  }
  String displayText = allEvents;
  if (displayText.length() > 18) {
    drawScrollingText(displayText.c_str(), 5, 12, 118, u8g2_font_t0_14_tr, 30);
  } else {
    drawCenteredStr(12, displayText.c_str(), u8g2_font_t0_14_tr);
  }

  // Time
  drawCenteredStr(42, timeStr, u8g2_font_logisoso24_tr);

  // Date & Sensors
  char combineStr[32];
  if (sensorStr[0] != '\0') {
    strftime(dateStr, sizeof(dateStr), "%a %d", &timeinfo);
    snprintf(combineStr, sizeof(combineStr), "%s %s", dateStr, sensorStr);
  } else {
    // --- Date formatting ---
    const char* fmt = mapDateFormat(clockSettings.dateFormat);
    strftime(dateStr, sizeof(dateStr), fmt, &timeinfo);
    snprintf(combineStr, sizeof(combineStr), "%s", dateStr);
  }
  drawCenteredStr(60, combineStr, u8g2_font_t0_15_tr);

  u8g2.sendBuffer();
}

// --- Theme 2: Minimal ---
void themeMinimal() {
  u8g2.clearBuffer();
  // Date & Time
  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);
  char timeStr[16] = "--:--";
  if (hasTime) {
    if (clockSettings.timeFormat == 12) {
      strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);  // 12h format
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // 24h format
    }
  }
  drawCenteredStr(52, timeStr, u8g2_font_logisoso42_tr);
  u8g2.sendBuffer();
}

// --- Theme 3: Classic 2 ---
void themeClassic2() {
  u8g2.clearBuffer();

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  char timeStr[12] = "--:--";       // for hh:hh
  char sStr[3] = "--";              // for ss
  char dateStr[16] = "--/--/----";  // for date
  char todayStr[6];                 // for events

  if (hasTime) {
    strftime(todayStr, sizeof(todayStr), "%m-%d", &timeinfo);  // For events
    if (clockSettings.timeFormat == 12) {
      strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);  // 12h format
      strftime(sStr, sizeof(sStr), "%S", &timeinfo);
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // 24h format
      strftime(sStr, sizeof(sStr), "%S", &timeinfo);
    }
  }

  // --- Events ---
  String allEvents = "";
  for (int i = 0; i < eventsCount; i++) {
    if (eventsList[i].date == String(todayStr)) {
      if (allEvents.length() > 0) allEvents += " | ";
      allEvents += eventsList[i].text;
    }
  }
  String displayText = allEvents;
  if (displayText.length() > 18) {
    drawScrollingText(displayText.c_str(), 5, 9, 118, u8g2_font_t0_11_tr, 30);
  } else {
    drawCenteredStr(9, displayText.c_str(), u8g2_font_t0_11_tr);
  }

  // --- Time ---
  drawCenteredStr(45, timeStr, u8g2_font_logisoso30_tr);  // for time HH:MM
  u8g2.setFont(u8g2_font_crox3h_tr);
  u8g2.drawStr(108, 27, sStr);  // for second SS


  // Date & Sensors
  char combineStr[32];
  if (sensorStr[0] != '\0') {
    strftime(dateStr, sizeof(dateStr), "%a %d", &timeinfo);
    snprintf(combineStr, sizeof(combineStr), "%s %s", dateStr, sensorStr);
  } else {
    // --- Date formatting ---
    const char* fmt = mapDateFormat(clockSettings.dateFormat);
    strftime(dateStr, sizeof(dateStr), fmt, &timeinfo);
    snprintf(combineStr, sizeof(combineStr), "%s", dateStr);
  }
  drawCenteredStr(61, combineStr, u8g2_font_t0_15_tr);

  u8g2.sendBuffer();
}


// --- Theme 4: Minimal Inverted ---
void themeMinimalInverted() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.setDrawColor(0);

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);
  char timeStr[16] = "--:--";
  if (hasTime) {
    if (clockSettings.timeFormat == 12) {
      strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);  // 12h format
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // 24h format
    }
  }
  drawCenteredStr(52, timeStr, u8g2_font_logisoso42_tr);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// --- Theme 5: Classic DTE Inverted ---
void themeClassicDTEInverted() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.setDrawColor(0);

  // Date & Time
  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  char dateStr[16] = "--/--/----";  // Date
  char timeStr[16] = "--:--:--";    // Time
  char todayStr[6];                 // "MM-DD"

  if (hasTime) {
    strftime(todayStr, sizeof(todayStr), "%m-%d", &timeinfo);  // For events
    if (clockSettings.timeFormat == 12) {
      strftime(timeStr, sizeof(timeStr), "%I:%M:%S", &timeinfo);  // 12h format
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);  // 24h format
    }
  }

  // --- Events ---
  String allEvents = "";
  for (int i = 0; i < eventsCount; i++) {
    if (eventsList[i].date == String(todayStr)) {
      if (allEvents.length() > 0) allEvents += " | ";
      allEvents += eventsList[i].text;
    }
  }
  String displayText = allEvents;
  if (displayText.length() > 18) {
    drawScrollingText(displayText.c_str(), 5, 12, 118, u8g2_font_t0_14_tr, 30);
  } else {
    drawCenteredStr(12, displayText.c_str(), u8g2_font_t0_14_tr);
  }

  // Time
  drawCenteredStr(42, timeStr, u8g2_font_logisoso24_tr);

  // Date & Sensors
  char combineStr[32];
  if (sensorStr[0] != '\0') {
    strftime(dateStr, sizeof(dateStr), "%a %d", &timeinfo);
    snprintf(combineStr, sizeof(combineStr), "%s %s", dateStr, sensorStr);
  } else {
    // --- Date formatting ---
    const char* fmt = mapDateFormat(clockSettings.dateFormat);
    strftime(dateStr, sizeof(dateStr), fmt, &timeinfo);
    snprintf(combineStr, sizeof(combineStr), "%s", dateStr);
  }

  drawCenteredStr(60, combineStr, u8g2_font_t0_15_tr);

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// ---Theme 6: Analog Clock (64x64)
void themeAnalogClock() {
  u8g2.clearBuffer();

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) return;

  char dateStr[16] = "--/--/----";
  char timeStr[16] = "--:--";
  char sStr[3] = "--";
  char todayStr[6];  // "MM-DD"

  strftime(todayStr, sizeof(todayStr), "%m-%d", &timeinfo);  // For events

  if (clockSettings.timeFormat == 12) {
    strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);  // 12h format
    strftime(sStr, sizeof(sStr), "%S", &timeinfo);
  } else {
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // 24h format
    strftime(sStr, sizeof(sStr), "%S", &timeinfo);
  }
  const char* fmt = mapDateFormat(clockSettings.dateFormat);  // For Date
  strftime(dateStr, sizeof(dateStr), fmt, &timeinfo);         // For Date

  // Center of dial (64x64 area left of screen)
  int cx = 32;
  int cy = 32;
  int r = 30;  // radius

  // --- Draw clock circle ---
  u8g2.drawCircle(cx, cy, r, U8G2_DRAW_ALL);

  // --- Draw tick marks (12 major) ---
  for (int i = 0; i < 12; i++) {
    float angle = i * 30 * M_PI / 180.0;  // 30° each
    int x1 = cx + cos(angle) * (r - 2);
    int y1 = cy + sin(angle) * (r - 2);

    int tickLen = (i % 3 == 0) ? 5 : 2;  // longer for 12,3,6,9
    int x2 = cx + cos(angle) * (r - tickLen);
    int y2 = cy + sin(angle) * (r - tickLen);

    // Draw normal tick
    u8g2.drawLine(x1, y1, x2, y2);

    // For 12,3,6,9 → draw extra parallel line for bold effect
    if (i % 3 == 0) {
      int x2b = cx + cos(angle) * (r - tickLen - 1);
      int y2b = cy + sin(angle) * (r - tickLen - 1);
      u8g2.drawLine(x1, y1, x2b, y2b);
    }
  }

  // --- Calculate angles ---
  float secAngle = (timeinfo.tm_sec * 6 - 90) * M_PI / 180.0;
  float minAngle = (timeinfo.tm_min * 6 + timeinfo.tm_sec * 0.1 - 90) * M_PI / 180.0;
  float hourAngle = ((timeinfo.tm_hour % 12) * 30 + timeinfo.tm_min * 0.5 - 90) * M_PI / 180.0;

  // --- Hour hand ---
  int hx = cx + cos(hourAngle) * (r - 14);
  int hy = cy + sin(hourAngle) * (r - 14);
  u8g2.drawLine(cx, cy, hx, hy);

  // --- Minute hand ---
  int mx = cx + cos(minAngle) * (r - 6);
  int my = cy + sin(minAngle) * (r - 6);
  u8g2.drawLine(cx, cy, mx, my);

  // --- Second hand ---
  static int lastSec = -1;
  if (secondHandBeep && timeinfo.tm_sec != lastSec) {
    lastSec = timeinfo.tm_sec;
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1);
    digitalWrite(BUZZER_PIN, LOW);
  }
  int sx = cx + cos(secAngle) * (r - 2);
  int sy = cy + sin(secAngle) * (r - 2);
  u8g2.drawLine(cx, cy, sx, sy);

  // --- Small center dot ---
  u8g2.drawDisc(cx, cy, 2, U8G2_DRAW_ALL);

  // --- Draw digital clock/date right side ---
  u8g2.setDrawColor(1);
  u8g2.drawRBox(68, 0, 60, 64, 2);  // white box for time
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_lastapprenticebold_tr);
  u8g2.drawStr(72, 15, timeStr);
  u8g2.setFont(u8g2_font_helvR12_tr);
  u8g2.drawStr(106, 15, sStr);

  int16_t strWidth;
  int16_t x;

  // --- Date ---
  if (strlen(dateStr) > 8) {
    strWidth = u8g2.getStrWidth(dateStr);
    x = 68 + ((60 - strWidth) / 2);
    drawScrollingText2(dateStr, 72, 31, 53, u8g2_font_lastapprenticebold_tr, 10);
  } else {
    u8g2.setFont(u8g2_font_lastapprenticebold_tr);
    strWidth = u8g2.getStrWidth(dateStr);
    x = 68 + ((60 - strWidth) / 2);
    u8g2.drawStr(x, 31, dateStr);
  }

  // --- Show temperatur/humidity
  if (sensorStr[0] != '\0') {
    u8g2.setFont(u8g2_font_t0_14_tr);
    strWidth = u8g2.getStrWidth(sensorStr);
    x = 68 + ((60 - strWidth) / 2);
    u8g2.drawStr(x, 47, sensorStr);
  }

  // --- Events ---
  String allEvents = "";
  for (int i = 0; i < eventsCount; i++) {
    if (eventsList[i].date == String(todayStr)) {
      if (allEvents.length() > 0) allEvents += " | ";
      allEvents += eventsList[i].text;
    }
  }
  String displayText = allEvents;
  drawScrollingText(displayText.c_str(), 72, 60, 53, u8g2_font_t0_14_tr, 30);

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// ---Theme 7: Detailed Information
void themeDetailedInformations() {
  u8g2.clearBuffer();

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  char timeStr[12] = "--:--";       // for time
  char sStr[3] = "--";              // for second
  char dateStr[16] = "--/--/----";  // for date
  char todayStr[6];                 // for events
  char dayStr[6];                   // for days
  char weekStr[6];                  // for week

  if (hasTime) {
    strftime(todayStr, sizeof(todayStr), "%m-%d", &timeinfo);  // For events
    if (clockSettings.timeFormat == 12) {
      strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);  // 12h format
      strftime(sStr, sizeof(sStr), "%S", &timeinfo);
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // 24h format
      strftime(sStr, sizeof(sStr), "%S", &timeinfo);
    }
    // --- Date formatting ---
    const char* fmt = mapDateFormat(clockSettings.dateFormat);
    strftime(dateStr, sizeof(dateStr), fmt, &timeinfo);
  }

  // --- Time ---
  u8g2.setFont(u8g2_font_timB24_tn);  // for hh:mm
  u8g2.drawStr(0, 26, timeStr);       // for hh:mm
  u8g2.setFont(u8g2_font_t0_11_tr);   // for ss
  u8g2.drawStr(78, 11, sStr);         // for ss

  // --- Total days ---
  if (clockSettings.totalDays) {
    strftime(dayStr, sizeof(dayStr), "%jD", &timeinfo);
    u8g2.drawFrame(96, 0, 32, 26);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(99, 12, dayStr);
  }

  // --- Week Number ---
  if (clockSettings.weekNumber) {
    strftime(weekStr, sizeof(weekStr), "%WW", &timeinfo);
    u8g2.drawFrame(96, 0, 32, 26);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(99, 23, weekStr);
  }

  // --- Date ---
  drawCenteredStr(38, dateStr, u8g2_font_t0_13_tr);

  // --- Weather Data ---
  if (sensorStr[0] != '\0') {
    u8g2.setFont(u8g2_font_t0_13_tr);
    drawCenteredStr(51, sensorStr, u8g2_font_t0_13_tr);
  } else {
    u8g2.setFont(u8g2_font_t0_13_tr);
    drawCenteredStr(51, "-- SENSOR OFF --", u8g2_font_t0_13_tr);
  }

  // --- Events ---
  String allEvents = "";
  for (int i = 0; i < eventsCount; i++) {
    if (eventsList[i].date == String(todayStr)) {
      if (allEvents.length() > 0) allEvents += " | ";
      allEvents += eventsList[i].text;
    }
  }
  String displayText = allEvents;
  if (displayText.length() > 18) {
    drawScrollingText(displayText.c_str(), 5, 61, 118, u8g2_font_t0_11_tr, 30);
  } else {
    drawCenteredStr(61, displayText.c_str(), u8g2_font_t0_11_tr);
  }
  u8g2.sendBuffer();
}

// ---Theme 8: Boxee
void themeBoxee() {
  u8g2.clearBuffer();
  u8g2.drawRFrame(0, 0, 128, 64, 3);  // outer box
  u8g2.drawRFrame(68, 3, 57, 58, 2);  // inner box

  u8g2.setDrawColor(1);
  u8g2.drawRBox(3, 3, 63, 25, 2);  // white box for time
  u8g2.setDrawColor(0);

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  char timeStr[12] = "--:--";       // for time
  char dateStr[16] = "--/--/----";  // for date
  char todayStr[6];                 // for events
  char weekStr[6];                  // for week

  if (hasTime) {
    strftime(todayStr, sizeof(todayStr), "%m-%d", &timeinfo);  // For events
    if (clockSettings.timeFormat == 12) {
      strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);  // 12h format
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // 24h format
    }

    strftime(weekStr, sizeof(weekStr), "%a", &timeinfo);
    // --- Date formatting ---
    const char* fmt = mapDateFormat(clockSettings.dateFormat);
    strftime(dateStr, sizeof(dateStr), fmt, &timeinfo);
  }

  // --- Time ---
  u8g2.setFont(u8g2_font_timB18_tr);  // for hh:mm
  u8g2.drawStr(5, 24, timeStr);       // for hh:mm

  int16_t strWidth;
  int16_t x;

  // --- Day ---
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_t0_13_tr);
  strWidth = u8g2.getStrWidth(weekStr);
  x = (68 - strWidth) / 2;
  u8g2.drawStr(x, 40, weekStr);

  // --- Date ---
  u8g2.setFont(u8g2_font_t0_13_tr);
  if (strlen(dateStr) > 8) {
    drawScrollingText2(dateStr, 4, 55, 63, u8g2_font_t0_13_tr, 10);
  } else {
    strWidth = u8g2.getStrWidth(dateStr);
    x = (68 - strWidth) / 2;
    u8g2.drawStr(x, 55, dateStr);
  }

  // --- Weather Data ---
  if (sensorStr[0] != '\0') {
    u8g2.setFont(u8g2_font_crox2h_tr);

    strWidth = u8g2.getStrWidth(tempStr);
    x = 68 + ((58 - strWidth) / 2);
    u8g2.drawStr(x, 20, tempStr);

    strWidth = u8g2.getStrWidth(humiStr);
    x = 68 + ((58 - strWidth) / 2);
    u8g2.drawStr(x, 37, humiStr);
  }

  // --- Events ---
  String allEvents = "";
  for (int i = 0; i < eventsCount; i++) {
    if (eventsList[i].date == String(todayStr)) {
      if (allEvents.length() > 0) allEvents += " | ";
      allEvents += eventsList[i].text;
    }
  }
  String displayText = allEvents;
  drawScrollingText(displayText.c_str(), 71, 55, 51, u8g2_font_t0_13_tr, 30);

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// ---Theme 8: Classic Boxee
void themeClassicBoxee() {
  u8g2.clearBuffer();
  u8g2.drawRFrame(0, 0, 128, 64, 3);  // outer box

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  char timeStr[12] = "--:--";       // for time HH:MM
  char sStr[6];                     // for second SS
  char dateStr[16] = "--/--/----";  // for date
  char todayStr[6];                 // for events
  char weekStr[6];                  // for week

  if (hasTime) {
    strftime(todayStr, sizeof(todayStr), "%m-%d", &timeinfo);  // For events
    if (clockSettings.timeFormat == 12) {
      strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);  // 12h format
      strftime(sStr, sizeof(sStr), "%S", &timeinfo);
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // 24h format
      strftime(sStr, sizeof(sStr), "%S", &timeinfo);
    }

    strftime(weekStr, sizeof(weekStr), "%a", &timeinfo);
    // --- Date formatting ---
    const char* fmt = mapDateFormat(clockSettings.dateFormat);
    strftime(dateStr, sizeof(dateStr), fmt, &timeinfo);
  }

  // --- Day ---
  u8g2.setFont(u8g2_font_crox2h_tr);
  u8g2.drawStr(3, 14, weekStr);

  // --- Events ---
  String allEvents = "";
  for (int i = 0; i < eventsCount; i++) {
    if (eventsList[i].date == String(todayStr)) {
      if (allEvents.length() > 0) allEvents += " | ";
      allEvents += eventsList[i].text;
    }
  }
  String displayText = allEvents;
  drawScrollingText(displayText.c_str(), 39, 14, 51, u8g2_font_t0_13_tr, 30);

  // --- Weather Data ---
  if (sensorStr[0] != '\0') {
    u8g2.setFont(u8g2_font_crox2h_tr);
    u8g2.drawStr(99, 14, tempStr2);
  }

  // --- Time ---
  drawCenteredStr(43, timeStr, u8g2_font_logisoso24_tf);
  u8g2.setFont(u8g2_font_crox2h_tr);  // for ss
  u8g2.drawStr(101, 43, sStr);

  // --- Date ---
  drawCenteredStr(58, dateStr, u8g2_font_crox2h_tr);

  u8g2.sendBuffer();
}

// ---Theme 9: Dial (64x64)
void themeDial() {
  u8g2.clearBuffer();

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  // --- Buzzer tick each new second ---
  static int lastSec = -1;
  if (secondHandBeep && timeinfo.tm_sec != lastSec) {
    lastSec = timeinfo.tm_sec;
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Center of dial (64x64 area left of screen)
  int cx = 64;
  int cy = 32;
  int r = 30;  // radius

  // --- Draw clock circle ---
  u8g2.drawCircle(cx, cy, r, U8G2_DRAW_ALL);

  // --- Draw tick marks (12 major) ---
  for (int i = 0; i < 12; i++) {
    float angle = i * 30 * M_PI / 180.0;  // 30° each
    int x1 = cx + cos(angle) * (r - 2);
    int y1 = cy + sin(angle) * (r - 2);

    int tickLen = (i % 3 == 0) ? 5 : 2;  // longer for 12,3,6,9
    int x2 = cx + cos(angle) * (r - tickLen);
    int y2 = cy + sin(angle) * (r - tickLen);

    // Draw normal tick
    u8g2.drawLine(x1, y1, x2, y2);

    // For 12,3,6,9 → draw extra parallel line for bold effect
    if (i % 3 == 0) {
      int x2b = cx + cos(angle) * (r - tickLen - 1);
      int y2b = cy + sin(angle) * (r - tickLen - 1);
      u8g2.drawLine(x1, y1, x2b, y2b);
    }
  }

  // --- Calculate angles ---
  float secAngle = (timeinfo.tm_sec * 6 - 90) * M_PI / 180.0;
  float minAngle = (timeinfo.tm_min * 6 + timeinfo.tm_sec * 0.1 - 90) * M_PI / 180.0;
  float hourAngle = ((timeinfo.tm_hour % 12) * 30 + timeinfo.tm_min * 0.5 - 90) * M_PI / 180.0;

  // --- Hour hand ---
  int hx = cx + cos(hourAngle) * (r - 14);
  int hy = cy + sin(hourAngle) * (r - 14);
  u8g2.drawLine(cx, cy, hx, hy);

  // --- Minute hand ---
  int mx = cx + cos(minAngle) * (r - 6);
  int my = cy + sin(minAngle) * (r - 6);
  u8g2.drawLine(cx, cy, mx, my);

  // --- Second hand ---
  int sx = cx + cos(secAngle) * (r - 2);
  int sy = cy + sin(secAngle) * (r - 2);
  u8g2.drawLine(cx, cy, sx, sy);

  // --- Small center dot ---
  u8g2.drawDisc(cx, cy, 2, U8G2_DRAW_ALL);


  // --- Show temperatur/humidity
  if (sensorStr[0] != '\0') {
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(105, 62, tempStr2);
  }

  u8g2.sendBuffer();
}


// ---Theme 10: Bar Clock (with floating values)
void themeBarClock() {
  u8g2.clearBuffer();

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  int hour = 0, hourTxt = 0, minute = 0, second = 0;
  char hourStr[3];
  char dateStr[16];
  char timeStr[16];
  char sStr[3];
  char todayStr[6];  // "MM-DD"

  if (hasTime) {
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;
    strftime(todayStr, sizeof(todayStr), "%m-%d", &timeinfo);  // For events

    if (clockSettings.timeFormat == 12) {
      strftime(hourStr, sizeof(hourStr), "%I", &timeinfo);     // 12h format
      strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);  // 12h format
      strftime(sStr, sizeof(sStr), "%S", &timeinfo);
      hourTxt = atoi(hourStr);
    } else {
      strftime(hourStr, sizeof(hourStr), "%H", &timeinfo);     // 24h format
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // 24h format
      strftime(sStr, sizeof(sStr), "%S", &timeinfo);
      hourTxt = atoi(hourStr);
    }
    const char* fmt = mapDateFormat(clockSettings.dateFormat);  // For Date
    strftime(dateStr, sizeof(dateStr), fmt, &timeinfo);         // For Date
  }

  // Layout
  const int barWidth = 18;
  const int barSpacing = 3;
  const int startX = 0;
  const int baseY = 62;
  const int maxBarHeight = 60;


  const int hourBarHeight = hasTime ? (hour * maxBarHeight) / 23 : 0;
  const int minBarHeight = hasTime ? (minute * maxBarHeight) / 59 : 0;
  const int secBarHeight = hasTime ? (second * maxBarHeight) / 59 : 0;

  u8g2.setFont(u8g2_font_lastapprenticebold_tr);

  // --- Helper for drawing a bar + label ---
  auto drawBar = [&](int x, int barHeight, int value) {
    int y = baseY - barHeight;
    u8g2.setDrawColor(1);
    u8g2.drawBox(x, y, barWidth, barHeight);

    char buf[4];
    sprintf(buf, "%02d", value);
    int txtW = u8g2.getStrWidth(buf);
    int txtX = x + (barWidth - txtW) / 2;
    int txtY = y - 2;
    // if (txtY < 10) txtY = 10;

    // --- Invert text only when near top ---
    if (y < 15) {
      u8g2.setDrawColor(0);
      u8g2.drawStr(txtX, y + 14, buf);  // inverted text
      u8g2.setDrawColor(1);             // restore
    } else {
      u8g2.drawStr(txtX, txtY, buf);
    }
  };

  // Bars
  drawBar(startX, hourBarHeight, hourTxt);
  drawBar(startX + barWidth + barSpacing, minBarHeight, minute);
  drawBar(startX + 2 * (barWidth + barSpacing), secBarHeight, second);

  // --- Draw digital clock/date right side ---
  u8g2.setDrawColor(1);
  u8g2.drawRBox(68, 0, 60, 64, 2);  // white box for time
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_lastapprenticebold_tr);
  u8g2.drawStr(72, 15, timeStr);
  u8g2.setFont(u8g2_font_helvR12_tr);
  u8g2.drawStr(106, 15, sStr);

  int16_t strWidth;
  int16_t x;

  // --- Date ---
  if (strlen(dateStr) > 8) {
    strWidth = u8g2.getStrWidth(dateStr);
    x = 68 + ((60 - strWidth) / 2);
    drawScrollingText2(dateStr, 72, 31, 53, u8g2_font_lastapprenticebold_tr, 10);
  } else {
    u8g2.setFont(u8g2_font_lastapprenticebold_tr);
    strWidth = u8g2.getStrWidth(dateStr);
    x = 68 + ((60 - strWidth) / 2);
    u8g2.drawStr(x, 31, dateStr);
  }

  // --- Show temperatur/humidity
  if (sensorStr[0] != '\0') {
    u8g2.setFont(u8g2_font_t0_14_tr);
    strWidth = u8g2.getStrWidth(sensorStr);
    x = 68 + ((60 - strWidth) / 2);
    u8g2.drawStr(x, 47, sensorStr);
  }

  // --- Events ---
  String allEvents = "";
  for (int i = 0; i < eventsCount; i++) {
    if (eventsList[i].date == String(todayStr)) {
      if (allEvents.length() > 0) allEvents += " | ";
      allEvents += eventsList[i].text;
    }
  }
  String displayText = allEvents;
  drawScrollingText(displayText.c_str(), 72, 60, 53, u8g2_font_t0_14_tr, 30);

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// ---Theme 11: Classic 3.0
void themeClassic3() {
  u8g2.clearBuffer();

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  char timeStr[16];
  char dateStr[16];
  char dayStr[6];
  char dayAndTempStr[50];

  if (hasTime) {
    if (clockSettings.timeFormat == 12) {
      strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);  // 12h format
    } else {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // 24h format
    }
    strftime(dateStr, sizeof(dateStr), "%b %d,  %Y", &timeinfo);  // For Date
    strftime(dayStr, sizeof(dayStr), "%a", &timeinfo);            // For Day
    snprintf(dayAndTempStr, sizeof(dayAndTempStr), "%s  %s", dayStr, tempStr2);
  }

  drawCenteredStr(9, dateStr, u8g2_font_t0_14_tr);
  drawCenteredStr(49, timeStr, u8g2_font_logisoso34_tr);
  drawCenteredStr(63, dayAndTempStr, u8g2_font_t0_14_tr);
  u8g2.sendBuffer();
}

void themeWeatherView() {
  u8g2.clearBuffer();
  // --- Weather Data --- 
  if (sensorStr[0] != '\0') {
    drawCenteredStr(27, tempStr, u8g2_font_helvR24_tr);
    drawCenteredStr(58, humiStr, u8g2_font_helvR18_tr);
  }
  u8g2.sendBuffer();
}

void showThemeMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = themeIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > themeMenuCount) end = themeMenuCount;
  for (int i = start; i < end; i++) {
    if (i == themeIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, themeMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, themeMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

void handleThemeMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      themeIndex--;
      if (themeIndex < 0) themeIndex = themeMenuCount - 1;
      showThemeMenu();
    } else if (btn == BTN_DOWN) {
      themeIndex++;
      if (themeIndex >= themeMenuCount) themeIndex = 0;
      showThemeMenu();
    } else if (btn == BTN_MENU) {
      waitForButtonRelease();
      if (themeIndex == 0) {
        themeActive = false;
        displaySettingsActive = true;
        showDisplaySettingsMenu();
      } else {
        // Preview selected theme
        int previewTheme = themeIndex - 1;
        showThemePreview(previewTheme);
        waitForButtonRelease();
        while (true) {
          ButtonType previewBtn = readButton();
          if (previewBtn == BTN_MENU) {
            buttonClickSound();
            waitForButtonRelease();
            // Confirmation dialog
            bool confirmed = false;
            showConfirmDialog("Save changes!", confirmed);
            waitForButtonRelease();
            if (confirmed) {
              selectedTheme = previewTheme;
              saveThemeSetting();
              showMessage("Theme Saved!", 44, 1200);
            } else {
              showMessage("Cancelled!", 44, 1200);
            }
            break;
          }
        }
        showThemeMenu();
      }
    }
  }
}

void showDisplaySettingsMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = displaySettingsIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > displaySettingsMenuCount) end = displaySettingsMenuCount;
  for (int i = start; i < end; i++) {
    if (i == displaySettingsIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, displaySettingsMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, displaySettingsMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

void handleDisplaySettingsMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      displaySettingsIndex--;
      if (displaySettingsIndex < 0) displaySettingsIndex = displaySettingsMenuCount - 1;
      showDisplaySettingsMenu();
    } else if (btn == BTN_DOWN) {
      displaySettingsIndex++;
      if (displaySettingsIndex >= displaySettingsMenuCount) displaySettingsIndex = 0;
      showDisplaySettingsMenu();
    } else if (btn == BTN_MENU) {
      waitForButtonRelease();
      if (displaySettingsIndex == 0) {
        displaySettingsActive = false;
        settingsActive = true;
        showSettingsMenu();
      } else if (displaySettingsIndex == 1) {
        displaySettingsActive = false;
        themeActive = true;
        themeIndex = 0;
        showThemeMenu();
      }
    }
  }
}

// -- Sound Settings --

void showSoundMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = soundIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > soundMenuCount) end = soundMenuCount;
  for (int i = start; i < end; i++) {
    if (i == soundIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, soundMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, soundMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

void handleSoundMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      soundIndex--;
      if (soundIndex < 0) soundIndex = soundMenuCount - 1;
      showSoundMenu();
    } else if (btn == BTN_DOWN) {
      soundIndex++;
      if (soundIndex >= soundMenuCount) soundIndex = 0;
      showSoundMenu();
    } else if (btn == BTN_MENU) {
      waitForButtonRelease();
      if (soundIndex == 0) {
        soundActive = false;
        settingsActive = true;
        showSettingsMenu();
      } else if (soundIndex == 1) {  // Buzzer
        bool confirmed = false;
        showConfirmDialog("Enable Buzzer?", confirmed);
        buzzerActive = confirmed;
        saveBuzzerSetting();
        showMessage(confirmed ? "Buzzer ON" : "Buzzer OFF", 44, 1200);
        loadBuzzerSetting();
        showSoundMenu();
      } else if (soundIndex == 2) {  // Second Hand Beep
        bool confirmed = false;
        showConfirmDialog("Enable Sec Beep?", confirmed);
        secondHandBeep = confirmed;
        saveBuzzerSetting();
        showMessage(confirmed ? "Sec Beep ON" : "Sec Beep OFF", 44, 1200);
        loadBuzzerSetting();
        showSoundMenu();
      }
    }
  }
}

// -- Storage Info --

void handleStorageInfo() {
  size_t total = SPIFFS.totalBytes();
  size_t used = SPIFFS.usedBytes();
  size_t free = total - used;

  char line1[32], line2[32], line3[32];
  sprintf(line1, "Total: %dKB", total / 1024);
  sprintf(line2, "Used : %dKB", used / 1024);
  sprintf(line3, "Free : %dKB", free / 1024);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);

  u8g2.drawStr(0, 12, "Storage Info:");
  u8g2.drawStr(0, 30, line1);
  u8g2.drawStr(0, 40, line2);
  u8g2.drawStr(0, 50, line3);

  u8g2.sendBuffer();
  delay(2500);
}

// -- Sensor --
void showSensorMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  int start = sensorIndex - 1;
  if (start < 0) start = 0;
  int end = start + 3;
  if (end > sensorMenuCount) end = sensorMenuCount;
  for (int i = start; i < end; i++) {
    if (i == sensorIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 13 + (i - start) * 14, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(2, 24 + (i - start) * 14, sensorMenu[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(2, 24 + (i - start) * 14, sensorMenu[i]);
    }
  }
  u8g2.sendBuffer();
}

void handleSensorMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      sensorIndex--;
      if (sensorIndex < 0) sensorIndex = sensorMenuCount - 1;
      showSensorMenu();
    } else if (btn == BTN_DOWN) {
      sensorIndex++;
      if (sensorIndex >= sensorMenuCount) sensorIndex = 0;
      showSensorMenu();
    } else if (btn == BTN_MENU) {
      waitForButtonRelease();
      switch (sensorIndex) {
        case 0:
          {  // Back
            sensorActive = false;
            settingsActive = true;
            showSettingsMenu();
            break;
          }
        case 1:
          {  // Temperature
            bool confirmed = false;
            showConfirmDialog("Turn ON Temperature?", confirmed);
            tempSensorOn = confirmed;
            saveSensorSettings();
            showMessage(confirmed ? "Temperature ON" : "Temperature OFF", 44, 1200);
            loadSensorSettings();
            showSensorMenu();
            break;
          }
        case 2:
          {  // Humidity
            bool confirmed = false;
            showConfirmDialog("Turn ON Humidity?", confirmed);
            humidSensorOn = confirmed;
            saveSensorSettings();
            showMessage(confirmed ? "Humidity ON" : "Humidity OFF", 44, 1200);
            loadSensorSettings();
            showSensorMenu();
            break;
          }
        case 3:
          {  // Human Detector
            showMessage("Not Available!", 44, 1200);
            loadSensorSettings();
            showSensorMenu();
            break;
          }
      }
    }
  }
}

// -- About --

void handleAboutInfo() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(0, 16, "ESP Study Clock");
  u8g2.drawStr(0, 28, "by M.Maity");
  u8g2.drawStr(0, 40, "Version: 1.0");
  u8g2.sendBuffer();
  delay(3000);
}

// -- Restart --

void handleRestart() {
  bool confirmed = false;
  showConfirmDialog("Restart ESP?", confirmed);
  if (confirmed) {
    showMessage("Restarting...", 44, 1200);
    ESP.restart();
  } else {
    showMessage("Cancelled!", 44, 1200);
  }
}


void handleSettingsMenu() {
  ButtonType btn = readButton();
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (btn != BTN_NONE && now - lastPress > 250) {  // debounce
    lastPress = now;
    buttonClickSound();
    if (btn == BTN_UP) {
      settingsIndex--;
      if (settingsIndex < 0) settingsIndex = settingsMenuCount - 1;
      showSettingsMenu();
    } else if (btn == BTN_DOWN) {
      settingsIndex++;
      if (settingsIndex >= settingsMenuCount) settingsIndex = 0;
      showSettingsMenu();
    } else if (btn == BTN_MENU) {
      // OK/Select
      waitForButtonRelease();

      if (settingsIndex == 0) {
        // Back to main menu
        settingsActive = false;
        menuActive = true;
        showMenu();
        delay(200);
      } else if (settingsIndex == 1) {
        settingsActive = false;
        wifiActive = true;
        wifiIndex = 0;
        showWiFiMenu();

      } else if (settingsIndex == 2) {  // Display Settings
        settingsActive = false;
        displaySettingsActive = true;
        displaySettingsIndex = 0;
        showDisplaySettingsMenu();

      } else if (settingsIndex == 3) {  // Sound Settings
        settingsActive = false;
        soundActive = true;
        soundIndex = 0;
        showSoundMenu();

      } else if (settingsIndex == 4) {  // Storage
        handleStorageInfo();
        showSettingsMenu();

      } else if (settingsIndex == 5) {  //Sensor
        settingsActive = false;
        sensorActive = true;
        sensorIndex = 0;
        showSensorMenu();

      } else if (settingsIndex == 6) {  // About
        handleAboutInfo();
        showSettingsMenu();

      } else if (settingsIndex == 7) {  // Restart
        handleRestart();
        showSettingsMenu();
      }
    }
  }
}


//////////////////////   WIFI SETUP   //////////////////////


bool connectToSavedWiFi() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 12, "Connecting WiFi...");
  u8g2.sendBuffer();

  WiFiManager wm;
  bool success = false;

  WiFi.mode(WIFI_STA);
  WiFi.begin();  // Try to connect to saved network

  int attempts = 0;
  const int MAX_ATTEMPTS = 5;

  while (attempts < MAX_ATTEMPTS) {

    char attemptStr[16];
    snprintf(attemptStr, 16, "Attempt: %d/5", attempts + 1);
    u8g2.drawStr(0, 24, attemptStr);
    u8g2.sendBuffer();

    if (WiFi.status() == WL_CONNECTED) {
      // Success - show connection details
      u8g2.clearBuffer();
      u8g2.drawStr(0, 12, "WiFi Connected!");
      u8g2.drawStr(0, 24, "SSID:");
      u8g2.drawStr(0, 36, WiFi.SSID().c_str());
      u8g2.drawStr(0, 48, "IP:");
      u8g2.drawStr(0, 60, WiFi.localIP().toString().c_str());
      u8g2.sendBuffer();
      delay(2000);
      return true;
    }

    delay(2000);
    attempts++;
  }

  // If not connected, start WiFiManager config portal
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "No Saved WiFi!");
  u8g2.drawStr(0, 24, "Starting AP...");
  u8g2.drawStr(0, 36, "AP IP:");
  u8g2.drawStr(0, 48, "192.168.4.1");
  u8g2.drawStr(0, 60, "Connect & Setup");
  u8g2.sendBuffer();
  delay(1500);

  wm.setConfigPortalTimeout(60);            // 60 seconds timeout
  success = wm.autoConnect("STUDY-CLOCK");  // Start AP

  if (success) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "WiFi Connected!");
    u8g2.drawStr(0, 24, "SSID:");
    u8g2.drawStr(0, 36, WiFi.SSID().c_str());
    u8g2.drawStr(0, 48, "IP:");
    u8g2.drawStr(0, 60, WiFi.localIP().toString().c_str());
    u8g2.sendBuffer();
    delay(2000);
    return true;
  } else {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "Time over!");
    u8g2.sendBuffer();
    delay(1000);
    return true;
  }
}


//////////////////////   WIFI STATUS   //////////////////////

void checkAndConnectWiFi() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);

  if (WiFi.status() == WL_CONNECTED) {
    // Already connected, show status
    u8g2.drawStr(0, 12, "Connected to:");
    u8g2.drawStr(0, 24, WiFi.SSID().c_str());
    u8g2.drawStr(0, 36, "IP:");
    u8g2.drawStr(0, 48, WiFi.localIP().toString().c_str());
    u8g2.sendBuffer();
    delay(3000);
  } else {
    // Try to reconnect
    u8g2.drawStr(0, 12, "Reconnecting to");
    u8g2.drawStr(0, 24, "saved network...");
    u8g2.sendBuffer();

    // Try to reconnect for 10 seconds
    WiFi.begin();
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(1000);
      attempts++;
      // Update progress
      u8g2.drawStr(0, 48, "Wait...");
      u8g2.drawStr(50, 48, String(10 - attempts).c_str());
      u8g2.drawStr(62, 48, "s");
      u8g2.sendBuffer();
    }

    if (WiFi.status() == WL_CONNECTED) {
      u8g2.clearBuffer();
      u8g2.drawStr(0, 12, "Connected!");
      u8g2.drawStr(0, 24, "SSID:");
      u8g2.drawStr(0, 36, WiFi.SSID().c_str());
      u8g2.drawStr(0, 48, "IP:");
      u8g2.drawStr(0, 60, WiFi.localIP().toString().c_str());
    } else {
      u8g2.clearBuffer();
      u8g2.drawStr(0, 24, "Connection Failed!");
      u8g2.drawStr(0, 36, "Goto Connect WiFi");
    }
    u8g2.sendBuffer();
    delay(3000);
  }
}


//////////////////////   TIME SETUP   //////////////////////

void configDateTime() {
  if (WiFi.status() != WL_CONNECTED) {
    u8g2.drawStr(0, 24, "No WiFi!");
    u8g2.sendBuffer();
    delay(1000);
    u8g2.drawStr(0, 36, "Time not synced!");
    u8g2.sendBuffer();
    delay(1000);
    u8g2.drawStr(0, 48, "Starting offline...");
    u8g2.sendBuffer();
    delay(1000);

    // ✅ Set default time: 12:00, 01/01/2025
    struct tm tm;
    tm.tm_year = 2025 - 1900;  // years since 1900
    tm.tm_mon = 0;             // January (0-based)
    tm.tm_mday = 1;            // Day
    tm.tm_hour = 12;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, nullptr);

    return;
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 12, "Syncing Time...");
  u8g2.sendBuffer();

  delay(1000);
  int attempts = 0;
  const int MAX_ATTEMPTS = 5;
  struct tm timeinfo;

  while (attempts < MAX_ATTEMPTS) {
    char attemptStr[16];
    snprintf(attemptStr, 16, "Attempt: %d/5", attempts + 1);
    u8g2.drawStr(0, 24, attemptStr);
    u8g2.sendBuffer();
    configTime(clockSettings.gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(2000);
    if (getLocalTime(&timeinfo)) {
      break;
    }
    attempts++;
  }

  if (getLocalTime(&timeinfo)) {
    char timeStr[16];
    char dateStr[18];
    char gmtStr[30];
    strftime(timeStr, sizeof(timeStr), "Time:  %H:%M:%S", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "Date:  %d.%m.%Y", &timeinfo);
    strftime(gmtStr, sizeof(gmtStr), "GMT :  %z %Z", &timeinfo);

    u8g2.clearBuffer();
    // u8g2.drawStr(0, 12, "Time Updated!");
    u8g2.setFont(u8g2_font_t0_14_tr);
    u8g2.drawStr(0, 16, timeStr);
    u8g2.drawStr(0, 32, dateStr);
    u8g2.drawStr(0, 62, gmtStr);
    u8g2.sendBuffer();
    delay(2000);
    // return;
  } else {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "Time not synced!");
    u8g2.drawStr(0, 24, "Check internet!");
    u8g2.drawStr(0, 48, "Starting offline...");
    u8g2.sendBuffer();
    delay(2000);

    // ✅ Apply default time if NTP fails
    struct tm tm;
    tm.tm_year = 2025 - 1900;  // years since 1900
    tm.tm_mon = 0;             // January
    tm.tm_mday = 1;            // Day
    tm.tm_hour = 12;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, nullptr);
  }
}

//////////////////////   WELCOME MESSAGE   //////////////////////


void welcomeMsg() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB18_tr);
  u8g2.drawStr(0, 22, MSG_WELCOME);
  u8g2.setFont(u8g2_font_ncenR12_tr);
  u8g2.drawStr(0, 40, MSG_SUBTITLE);
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(2, 60, MSG_DEVELOPER);
  u8g2.sendBuffer();
}

//////////////////////   HOURLY CHIME   //////////////////////

void checkHourlyChime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  // Check if it's at the start of an hour (00 minutes only)
  static int lastChimeHour = -1;  // Track last hour we chimed
  if (timeinfo.tm_min == 0 && timeinfo.tm_hour != lastChimeHour) {
    // Single beep for one second
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1500);

    digitalWrite(BUZZER_PIN, LOW);

    lastChimeHour = timeinfo.tm_hour;  // Update last chime hour
    Serial.printf("Hourly chime at %02d:00\n", timeinfo.tm_hour);
  }

  // Reset lastChimeHour when minute changes from 0
  if (timeinfo.tm_min != 0) {
    lastChimeHour = -1;
  }
}

//////////////////////   SETUP   //////////////////////

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_ADC_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  u8g2.begin();

  // --- Startup Beep ---
  digitalWrite(BUZZER_PIN, HIGH);
  delay(30);
  digitalWrite(BUZZER_PIN, LOW);

  // --- Welcome ---
  welcomeMsg();
  delay(2000);

  // --- Storage Init ---
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_14_tr);
  u8g2.drawStr(0, 18, "Settings Init:");
  u8g2.sendBuffer();
  delay(500);
  EEPROM.begin(512);

  loadClockSettings();
  loadAlarmSettings();
  loadTimerSettings();
  loadBuzzerSetting();
  loadThemeSetting();
  loadSensorSettings();
  u8g2.drawStr(0, 18, "Settings Init: OK");
  u8g2.sendBuffer();

  timerSetMillis = (timerSettings.hours * 3600000UL) + (timerSettings.minutes * 60000UL) + (timerSettings.seconds * 1000UL);
  timerRemainingMillis = timerSetMillis;
  delay(500);

  // --- SPIFFS ---
  u8g2.clearBuffer();
  if (!SPIFFS.begin(true)) {
    u8g2.drawStr(0, 18, "SPIFFS: ERROR");
  } else {
    u8g2.drawStr(0, 18, "SPIFFS: OK");
  }
  u8g2.sendBuffer();
  delay(1000);

  // --- Sensor Init ---
  u8g2.clearBuffer();
  if (!aht.begin()) {
    u8g2.drawStr(0, 18, "AHT10 Sensor: ERROR");
  } else {
    u8g2.drawStr(0, 18, "AHT10 Sensor: OK");
  }
  u8g2.sendBuffer();
  delay(1000);

  // --- Load Events ---
  u8g2.clearBuffer();
  u8g2.drawStr(0, 18, "Loading Events...");
  u8g2.sendBuffer();
  loadEventsFromJson("/events.json");
  delay(500);

  // --- WiFi Setup ---
  connectToSavedWiFi();
  delay(500);

  // --- Time Sync ---
  configDateTime();
  delay(1500);

  // --- Ready ---
  u8g2.clearBuffer();
  drawCenteredStr(35, "System Ready!", u8g2_font_t0_14_tr);
  u8g2.sendBuffer();
  delay(1500);

  u8g2.clearBuffer();
}

//////////////////////   LOOP   //////////////////////


void loop() {
  updateSensors();  // runs every 2s without blocking UI
  checkHourlyChime();
  checkAlarms();
  if (alarmTriggered) {
    handleAlarmTriggerUI();
    return;  // Don't run other UI while alarm is active
  }

  if (clockSettingsActive) {  // Clock Settings
    showClockSettingsMenu();
    handleClockSettingsMenu();
  } else if (alarmsActive) {  // Alarm
    showAlarmsMenu();
    handleAlarmsMenu();
  } else if (stopwatchActive) {  // Stopwatch
    showStopwatchMenu();
    handleStopwatchMenu();
  } else if (timerActive) {  // Timer
    showTimerMenu();
    handleTimerMenu();
  } else if (weatherActive) {  // Weather
    showWeatherMenu();
    handleWeatherMenu();
  } else if (newsActive) {  // News
    showNewsMenu();
    handleNewsMenu();
  } else if (eventsActive) {  // Events
    showEventsMenu();
    handleEventsMenu();
  } else if (settingsActive) {  // Settings
    showSettingsMenu();
    handleSettingsMenu();
  } else if (wifiActive) {  // WiFi Setting
    showWiFiMenu();
    handleWiFiMenu();
  } else if (displaySettingsActive) {  // Display Settings
    showDisplaySettingsMenu();
    handleDisplaySettingsMenu();
  } else if (themeActive) {  // Themes
    showThemeMenu();
    handleThemeMenu();
  } else if (soundActive) {  // Sound
    showSoundMenu();
    handleSoundMenu();
  } else if (sensorActive) {  // Sensor
    showSensorMenu();
    handleSensorMenu();
  } else if (menuActive) {
    showMenu();
    handleMenu();
  } else {
    switch (selectedTheme) {
      case 0: themeClassicTimeDateEvents(); break;
      case 1: themeMinimal(); break;
      case 2: themeClassic2(); break;
      case 3: themeMinimalInverted(); break;
      case 4: themeClassicDTEInverted(); break;
      case 5: themeAnalogClock(); break;
      case 6: themeDetailedInformations(); break;
      case 7: themeBoxee(); break;
      case 8: themeClassicBoxee(); break;
      case 9: themeDial(); break;
      case 10: themeBarClock(); break;
      case 11: themeClassic3(); break;
      case 12: themeWeatherView(); break;
      default: themeClassicTimeDateEvents(); break;
    }
    ButtonType btn = readButton();
    if (btn == BTN_MENU) {
      buttonClickSound();
      menuActive = true;
      menuIndex = 0;
      showMenu();
      waitForButtonRelease();
    } else if (btn == BTN_UP) {
      buttonClickSound();
      shortcutStopwatchActive = true;
      waitForButtonRelease();
    } else if (btn == BTN_DOWN) {
      buttonClickSound();
      shortcutTimerActive = true;
      waitForButtonRelease();
    }
  }
  // Handle shortcut UIs
  if (shortcutStopwatchActive) {
    handleShortcutStopwatchUI();
    // After exit, redraw main UI
    return;
  }
  if (shortcutTimerActive) {
    handleShortcutTimerUI();
    // After exit, redraw main UI
    return;
  }
}
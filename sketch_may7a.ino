#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// TFT pins
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2

// Buttons
#define BTN_UP      32
#define BTN_DOWN    33
#define BTN_ENTER   25
#define BTN_BACK    26   // physical back button, shown on screen as "#"

// Buzzer
#define BUZZER_PIN  21

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

String menuItems[] = {
  "LTE4-2000",
  "LTE4-400",
  "LTH800",
  "LTHH100",
  "LTC Various",
  "LTHM1500"
};

const int menuSize = 6;
int selected = 0;

// UI states
enum ScreenState {
  SCREEN_MAIN_MENU,
  SCREEN_TEST_MENU,
  SCREEN_DETAIL_SCREEN,
  SCREEN_AUX_TEST,
  SCREEN_AUTO_COIL_TEST
};

ScreenState currentScreen = SCREEN_MAIN_MENU;
int testSelected = 0;

// -----------------------------------------------------------------------------
// PWM wire test
// Outputs send different duty cycles:
//   GPIO12 -> 20%
//   GPIO13 -> 40%
//   GPIO14 -> 60%
//   GPIO27 -> 80%
//
// Inputs receive them:
//   GPIO15, GPIO16, GPIO17, GPIO19
//
// Expected mapping:
//   AUX1 NC -> OUT 12 -> IN 15
//   AUX1 NO -> OUT 13 -> IN 16
//   AUX2 NC -> OUT 14 -> IN 17
//   AUX2 NO -> OUT 27 -> IN 19
// -----------------------------------------------------------------------------

const uint8_t NUM_WIRES = 4;

const uint8_t outPins[NUM_WIRES] = {12, 13, 14, 27};
const uint8_t inPins[NUM_WIRES]  = {15, 16, 17, 19};
const uint8_t dutyPercent[NUM_WIRES] = {20, 40, 60, 80};

const unsigned long testRefreshMs = 250;
unsigned long lastTestUpdate = 0;

// Alarm/buzzer state
enum AlarmState {
  ALARM_NONE,
  ALARM_SWAP,
  ALARM_BAD
};

AlarmState currentAlarm = ALARM_NONE;
bool buzzerOutputOn = false;
unsigned long buzzerLastToggle = 0;

// Alarm timing
const unsigned long swapOnMs = 300;
const unsigned long swapOffMs = 700;
const unsigned long badOnMs = 120;
const unsigned long badOffMs = 120;

enum WireStatus {
  STATUS_OKAY,
  STATUS_SWAPED,
  STATUS_BAD
};

// ---------- function prototypes ----------
void drawMenu();
void drawTestMenu();
void showSelected();
void drawDetailScreen();
void drawAuxTestScreen();
void drawAutoCoilScreen();
void updateAuxTestScreen();

void setupPwmOutputs();
void setupBuzzer();

int readDutyPercent(uint8_t pin);
int matchExpectedWire(int measuredDuty);
WireStatus getWireStatus(uint8_t wireIndex, int measuredDuty);
const char* statusText(WireStatus s);

AlarmState getOverallAlarmState(WireStatus status[NUM_WIRES]);
void updateBuzzer(AlarmState state);
void buzzerOn(AlarmState state);
void buzzerOff();

void playUiTone(uint32_t freq, uint16_t durationMs);
void beepUp();
void beepDown();
void beepEnter();
void beepBack();

void waitForRelease(uint8_t pin);

// ---------- setup ----------
void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  // Inputs are pulldown so disconnected wires do not float.
  for (uint8_t i = 0; i < NUM_WIRES; i++) {
    pinMode(inPins[i], INPUT_PULLDOWN);
  }

  setupPwmOutputs();
  setupBuzzer();

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);

  drawMenu();
}

// ---------- loop ----------
void loop() {
  switch (currentScreen) {
    case SCREEN_MAIN_MENU:
      if (digitalRead(BTN_DOWN) == LOW) {
        selected = (selected + 1) % menuSize;
        beepDown();
        drawMenu();
        waitForRelease(BTN_DOWN);
      }

      if (digitalRead(BTN_UP) == LOW) {
        selected--;
        if (selected < 0) selected = menuSize - 1;
        beepUp();
        drawMenu();
        waitForRelease(BTN_UP);
      }

      if (digitalRead(BTN_ENTER) == LOW) {
        beepEnter();
        waitForRelease(BTN_ENTER);

        if (selected == 0) {
          currentScreen = SCREEN_TEST_MENU;
          testSelected = 0;
          drawTestMenu();
        } else {
          currentScreen = SCREEN_DETAIL_SCREEN;
          drawDetailScreen();
        }
      }
      break;

    case SCREEN_TEST_MENU:
      if (digitalRead(BTN_DOWN) == LOW) {
        testSelected = (testSelected + 1) % 2;
        beepDown();
        drawTestMenu();
        waitForRelease(BTN_DOWN);
      }

      if (digitalRead(BTN_UP) == LOW) {
        testSelected--;
        if (testSelected < 0) testSelected = 1;
        beepUp();
        drawTestMenu();
        waitForRelease(BTN_UP);
      }

      if (digitalRead(BTN_ENTER) == LOW) {
        beepEnter();
        waitForRelease(BTN_ENTER);

        if (testSelected == 0) {
          currentScreen = SCREEN_AUX_TEST;
          drawAuxTestScreen();
          lastTestUpdate = 0;
        } else {
          currentScreen = SCREEN_AUTO_COIL_TEST;
          drawAutoCoilScreen();
        }
      }

      if (digitalRead(BTN_BACK) == LOW) {
        beepBack();
        waitForRelease(BTN_BACK);
        currentScreen = SCREEN_MAIN_MENU;
        drawMenu();
      }
      break;

    case SCREEN_DETAIL_SCREEN:
      if (digitalRead(BTN_BACK) == LOW) {
        beepBack();
        waitForRelease(BTN_BACK);
        currentScreen = SCREEN_MAIN_MENU;
        drawMenu();
      }
      break;

    case SCREEN_AUX_TEST:
      if (digitalRead(BTN_BACK) == LOW) {
        beepBack();
        waitForRelease(BTN_BACK);

        updateBuzzer(ALARM_NONE);
        currentAlarm = ALARM_NONE;
        buzzerOff();
        lastTestUpdate = 0;

        currentScreen = SCREEN_TEST_MENU;
        drawTestMenu();
        break; // stop here so AUX screen does not redraw over the menu
      }

      if (millis() - lastTestUpdate >= testRefreshMs) {
        lastTestUpdate = millis();
        updateAuxTestScreen();
      }
      break;

    case SCREEN_AUTO_COIL_TEST:
      if (digitalRead(BTN_BACK) == LOW) {
        beepBack();
        waitForRelease(BTN_BACK);
        currentScreen = SCREEN_TEST_MENU;
        drawTestMenu();
      }
      break;
  }
}

// ---------- PWM setup ----------
void setupPwmOutputs() {
  // Arduino-ESP32 3.x LEDC API:
  // ledcAttach(pin, freq, resolution), then ledcWrite(pin, duty)
  for (uint8_t i = 0; i < NUM_WIRES; i++) {
    pinMode(outPins[i], OUTPUT);

    bool ok = ledcAttach(outPins[i], 1000, 8); // 1 kHz, 8-bit resolution
    if (!ok) {
      continue;
    }

    uint8_t dutyValue = map(dutyPercent[i], 0, 100, 0, 255);
    ledcWrite(outPins[i], dutyValue);
  }
}

// ---------- buzzer setup ----------
void setupBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);

  // Attach once. Tone frequency is changed later with ledcWriteTone().
  ledcAttach(BUZZER_PIN, 2000, 8);
  buzzerOff();
}

// ---------- menu drawing ----------
void drawMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.println("CONTACTORS");

  for (int i = 0; i < menuSize; i++) {
    int y = 30 + (i * 18);
    if (i == selected) {
      tft.fillRect(0, y - 2, 160, 18, ST77XX_YELLOW);
      tft.setTextColor(ST77XX_BLACK);
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }
    tft.setTextSize(1);
    tft.setCursor(10, y + 2);
    tft.println(menuItems[i]);
  }
}

void drawTestMenu() {
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(8, 5);
  tft.println("LTE4-2000");

  tft.drawLine(8, 28, 150, 28, ST77XX_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  if (testSelected == 0) {
    tft.fillRect(0, 40, 160, 16, ST77XX_YELLOW);
    tft.setTextColor(ST77XX_BLACK);
  } else {
    tft.setTextColor(ST77XX_WHITE);
  }
  tft.setCursor(10, 42);
  tft.println("Run AUX test");

  if (testSelected == 1) {
    tft.fillRect(0, 58, 160, 16, ST77XX_YELLOW);
    tft.setTextColor(ST77XX_BLACK);
  } else {
    tft.setTextColor(ST77XX_WHITE);
  }
  tft.setCursor(10, 60);
  tft.println("Run Auto coil test");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 100);
  tft.println("ENTER = SELECT");
  tft.setCursor(10, 112);
  tft.println("# = BACK");
}

void drawDetailScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println(menuItems[selected]);

  tft.drawLine(10, 34, 150, 34, ST77XX_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(10, 52);
  tft.println("No test defined yet");

  tft.setCursor(10, 110);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("# = BACK");
}

// ---------- AUX test screen ----------
void drawAuxTestScreen() {
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(8, 6);
  tft.println("LTE4-2000 AUX TEST");
  tft.drawLine(8, 16, 150, 16, ST77XX_WHITE);

  // Headers
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(58, 26);
  tft.print("NC");
  tft.setCursor(110, 26);
  tft.print("NO");

  // Row labels
  tft.setCursor(8, 42);
  tft.print("AUX1");
  tft.setCursor(8, 58);
  tft.print("AUX2");

  // Placeholders
  tft.setCursor(58, 42);
  tft.print("-");
  tft.setCursor(110, 42);
  tft.print("-");
  tft.setCursor(58, 58);
  tft.print("-");
  tft.setCursor(110, 58);
  tft.print("-");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 82);
  tft.println("OKAY = correct wire");
  tft.setCursor(10, 94);
  tft.println("SWAPED = other wire");
  tft.setCursor(10, 106);
  tft.println("BAD = no valid match");

  tft.setCursor(10, 118);
  tft.print("# = BACK");
}

void updateAuxTestScreen() {
  int measured[NUM_WIRES];
  WireStatus status[NUM_WIRES];

  for (uint8_t i = 0; i < NUM_WIRES; i++) {
    measured[i] = readDutyPercent(inPins[i]);
    status[i] = getWireStatus(i, measured[i]);
  }

  AlarmState alarm = getOverallAlarmState(status);
  updateBuzzer(alarm);

  // Clear only the status area
  tft.fillRect(48, 38, 110, 35, ST77XX_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  // AUX1 row
  tft.setCursor(58, 42);
  tft.print(statusText(status[0]));  // AUX1 NC
  tft.setCursor(110, 42);
  tft.print(statusText(status[1]));  // AUX1 NO

  // AUX2 row
  tft.setCursor(58, 58);
  tft.print(statusText(status[2]));  // AUX2 NC
  tft.setCursor(110, 58);
  tft.print(statusText(status[3]));  // AUX2 NO
}

// ---------- Auto coil test screen ----------
void drawAutoCoilScreen() {
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("AUTO COIL");

  tft.drawLine(10, 34, 150, 34, ST77XX_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 52);
  tft.println("Auto coil test menu");
  tft.setCursor(10, 64);
  tft.println("Logic can be added here");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 110);
  tft.println("# = BACK");
}

// ---------- wire reading ----------
int readDutyPercent(uint8_t pin) {
  const unsigned long sampleWindowUs = 20000; // 20 ms
  const unsigned int sampleDelayUs = 100;     // 100 us

  uint32_t highCount = 0;
  uint32_t totalCount = 0;

  unsigned long start = micros();
  while ((micros() - start) < sampleWindowUs) {
    if (digitalRead(pin) == HIGH) {
      highCount++;
    }
    totalCount++;
    delayMicroseconds(sampleDelayUs);
  }

  if (totalCount == 0) return 0;
  return (int)((highCount * 100UL) / totalCount);
}

int matchExpectedWire(int measuredDuty) {
  const int tolerance = 8; // percent

  for (int i = 0; i < NUM_WIRES; i++) {
    if (abs(measuredDuty - dutyPercent[i]) <= tolerance) {
      return i;
    }
  }

  return -1;
}

WireStatus getWireStatus(uint8_t wireIndex, int measuredDuty) {
  int matchedIndex = matchExpectedWire(measuredDuty);

  if (matchedIndex == -1) {
    return STATUS_BAD;
  }

  if (matchedIndex == (int)wireIndex) {
    return STATUS_OKAY;
  }

  return STATUS_SWAPED;
}

const char* statusText(WireStatus s) {
  switch (s) {
    case STATUS_OKAY:   return "OKAY";
    case STATUS_SWAPED:  return "SWAPED";
    case STATUS_BAD:    return "BAD";
    default:            return "?";
  }
}

// ---------- alarm logic ----------
AlarmState getOverallAlarmState(WireStatus status[NUM_WIRES]) {
  bool hasSwap = false;

  for (uint8_t i = 0; i < NUM_WIRES; i++) {
    if (status[i] == STATUS_BAD) {
      return ALARM_BAD;
    }
    if (status[i] == STATUS_SWAPED) {
      hasSwap = true;
    }
  }

  if (hasSwap) {
    return ALARM_SWAP;
  }

  return ALARM_NONE;
}

void buzzerOn(AlarmState state) {
  uint32_t freq = (state == ALARM_BAD) ? 2400 : 1400;
  ledcWriteTone(BUZZER_PIN, freq);
  buzzerOutputOn = true;
}

void buzzerOff() {
  ledcWriteTone(BUZZER_PIN, 0);
  buzzerOutputOn = false;
}

void updateBuzzer(AlarmState state) {
  if (state == ALARM_NONE) {
    buzzerOff();
    currentAlarm = ALARM_NONE;
    buzzerLastToggle = millis();
    return;
  }

  unsigned long now = millis();

  if (state != currentAlarm) {
    currentAlarm = state;
    buzzerLastToggle = now;
    buzzerOutputOn = false;
  }

  unsigned long intervalOn = (state == ALARM_BAD) ? badOnMs : swapOnMs;
  unsigned long intervalOff = (state == ALARM_BAD) ? badOffMs : swapOffMs;

  if (buzzerOutputOn) {
    if (now - buzzerLastToggle >= intervalOn) {
      buzzerOff();
      buzzerLastToggle = now;
    }
  } else {
    if (now - buzzerLastToggle >= intervalOff) {
      buzzerOn(state);
      buzzerLastToggle = now;
    }
  }
}

// ---------- UI tones ----------
void playUiTone(uint32_t freq, uint16_t durationMs) {
  buzzerOff();
  ledcWriteTone(BUZZER_PIN, freq);
  delay(durationMs);
  buzzerOff();
}

void beepUp() {
  playUiTone(2500, 60);
}

void beepDown() {
  playUiTone(1800, 60);
}

void beepEnter() {
  playUiTone(1200, 80);
  playUiTone(1600, 80);
}

void beepBack() {
  playUiTone(800, 80);
}

// ---------- helper ----------
void waitForRelease(uint8_t pin) {
  while (digitalRead(pin) == LOW) {
    delay(10);
  }
  delay(30);
}

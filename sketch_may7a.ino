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
#define BTN_BACK    26   // physical back button, shown on screen as "*"

// Buzzer
#define BUZZER_PIN  21

// Coil energise output
#define COIL_PIN    22

// Screen size
const int SCREEN_W = 128;
const int SCREEN_H = 160;

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
  SCREEN_WELCOME,
  SCREEN_MAIN_MENU,
  SCREEN_TEST_MENU,
  SCREEN_DETAIL_SCREEN,
  SCREEN_AUX_TEST,
  SCREEN_AUTO_COIL_TEST
};

ScreenState currentScreen = SCREEN_WELCOME;
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

// NO filter settings
const uint8_t NO_SAMPLE_COUNT = 6;
const uint8_t NO_REQUIRED_HITS = 4;
const int NO_PWM_THRESHOLD = 15;
const int NO_MAX_SPREAD = 18;

// Coil state
bool coilEnergised = false;

// ---------- function prototypes ----------
void drawWelcomeScreen();
void drawMenu();
void drawTestMenu();
void drawDetailScreen();
void drawAuxTestScreen();
void drawAutoCoilScreen();
void updateAuxTestScreen();
void updateAutoCoilAuxStatus();

void setupPwmOutputs();
void setupBuzzer();
void setupCoil();

int readDutyPercent(uint8_t pin);
int matchExpectedWire(int measuredDuty);
bool noContactHasSignal(uint8_t pin);
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

bool usesSharedTestMenu(int index);
void energiseCoil();
void deenergiseCoil();

// ---------- setup ----------
void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  for (uint8_t i = 0; i < NUM_WIRES; i++) {
    pinMode(inPins[i], INPUT_PULLDOWN);
  }

  setupPwmOutputs();
  setupBuzzer();
  setupCoil();

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);

  drawWelcomeScreen();
}

// ---------- loop ----------
void loop() {
  switch (currentScreen) {
    case SCREEN_WELCOME:
      if (digitalRead(BTN_ENTER) == LOW) {
        beepEnter();
        waitForRelease(BTN_ENTER);

        currentScreen = SCREEN_MAIN_MENU;
        selected = 0;
        drawMenu();
      }
      break;

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

        if (usesSharedTestMenu(selected)) {
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
          deenergiseCoil();
          drawAutoCoilScreen();
          lastTestUpdate = 0;
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
        break;
      }

      if (millis() - lastTestUpdate >= testRefreshMs) {
        lastTestUpdate = millis();
        updateAuxTestScreen();
      }
      break;

    case SCREEN_AUTO_COIL_TEST:
      if (digitalRead(BTN_ENTER) == LOW) {
        beepEnter();
        waitForRelease(BTN_ENTER);

        if (coilEnergised) {
          deenergiseCoil();
        } else {
          energiseCoil();
        }
        drawAutoCoilScreen();
      }

      if (millis() - lastTestUpdate >= testRefreshMs) {
        lastTestUpdate = millis();
        updateAutoCoilAuxStatus();
      }

      if (digitalRead(BTN_BACK) == LOW) {
        beepBack();
        waitForRelease(BTN_BACK);
        deenergiseCoil();
        currentScreen = SCREEN_TEST_MENU;
        drawTestMenu();
      }
      break;
  }
}

// ---------- shared menu logic ----------
bool usesSharedTestMenu(int index) {
  return (index == 0 || index == 2 || index == 3 || index == 4);
}

// ---------- PWM setup ----------
void setupPwmOutputs() {
  for (uint8_t i = 0; i < NUM_WIRES; i++) {
    pinMode(outPins[i], OUTPUT);

    bool ok = ledcAttach(outPins[i], 1000, 8);
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
  ledcAttach(BUZZER_PIN, 2000, 8);
  buzzerOff();
}

// ---------- coil setup ----------
void setupCoil() {
  pinMode(COIL_PIN, OUTPUT);
  deenergiseCoil();
}

void energiseCoil() {
  digitalWrite(COIL_PIN, HIGH);
  coilEnergised = true;
}

void deenergiseCoil() {
  digitalWrite(COIL_PIN, LOW);
  coilEnergised = false;
}

// ---------- welcome screen ----------
void drawWelcomeScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(24, 30);
  tft.println("WELCOME");

  tft.drawLine(12, 66, 116, 66, ST77XX_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(26, 88);
  tft.println("Press #");
}

// ---------- menu drawing ----------
void drawMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(4, 6);
  tft.println("CONTACTORS");

  for (int i = 0; i < menuSize; i++) {
    int y = 28 + (i * 18);

    if (i == selected) {
      tft.fillRect(0, y - 2, SCREEN_W, 16, ST77XX_YELLOW);
      tft.setTextColor(ST77XX_BLACK);
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }

    tft.setTextSize(1);
    tft.setCursor(8, y + 1);
    tft.println(menuItems[i]);
  }
}

void drawTestMenu() {
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(8, 5);
  tft.println(menuItems[selected]);

  tft.drawLine(8, 28, 120, 28, ST77XX_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  if (testSelected == 0) {
    tft.fillRect(0, 40, SCREEN_W, 16, ST77XX_YELLOW);
    tft.setTextColor(ST77XX_BLACK);
  } else {
    tft.setTextColor(ST77XX_WHITE);
  }
  tft.setCursor(10, 42);
  tft.println("Run AUX test");

  if (testSelected == 1) {
    tft.fillRect(0, 58, SCREEN_W, 16, ST77XX_YELLOW);
    tft.setTextColor(ST77XX_BLACK);
  } else {
    tft.setTextColor(ST77XX_WHITE);
  }
  tft.setCursor(10, 60);
  tft.println("Automatic function");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 100);
  tft.println("# = SELECT");
  tft.setCursor(10, 112);
  tft.println("* = BACK");
}

void drawDetailScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println(menuItems[selected]);

  tft.drawLine(10, 34, 118, 34, ST77XX_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(10, 52);
  tft.println("No test defined yet");

  tft.setCursor(10, 120);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("* = BACK");
}

// ---------- AUX test screen ----------
void drawAuxTestScreen() {
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(8, 6);
  tft.println("LTE4-2000 AUX TEST");
  tft.drawLine(8, 16, 120, 16, ST77XX_WHITE);

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(52, 26);
  tft.print("NC");
  tft.setCursor(96, 26);
  tft.print("NO");

  tft.setCursor(8, 42);
  tft.print("AUX1");
  tft.setCursor(8, 58);
  tft.print("AUX2");

  tft.setCursor(52, 42);
  tft.print("-");
  tft.setCursor(96, 42);
  tft.print("-");
  tft.setCursor(52, 58);
  tft.print("-");
  tft.setCursor(96, 58);
  tft.print("-");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 82);
  tft.println("OKAY = correct wire");
  tft.setCursor(10, 94);
  tft.println("SWAPED = other wire");
  tft.setCursor(10, 106);
  tft.println("BAD = no valid match");

  tft.setCursor(10, 120);
  tft.print("* = BACK");
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

  tft.fillRect(48, 38, 80, 35, ST77XX_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(52, 42);
  tft.print(statusText(status[0]));
  tft.setCursor(96, 42);
  tft.print(statusText(status[1]));

  tft.setCursor(52, 58);
  tft.print(statusText(status[2]));
  tft.setCursor(96, 58);
  tft.print(statusText(status[3]));
}

// ---------- Auto coil test screen ----------
void drawAutoCoilScreen() {
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(8, 6);
  tft.println("AUTO COIL");

  tft.drawLine(8, 16, 120, 16, ST77XX_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 30);
  if (coilEnergised) {
    tft.print("Status: ENERGISED");
  } else {
    tft.print("Status: DE-ENERGISED");
  }

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(52, 44);
  tft.print("NC");
  tft.setCursor(96, 44);
  tft.print("NO");

  tft.setCursor(8, 60);
  tft.print("AUX1");
  tft.setCursor(8, 76);
  tft.print("AUX2");

  tft.setCursor(52, 60);
  tft.print("-");
  tft.setCursor(96, 60);
  tft.print("-");
  tft.setCursor(52, 76);
  tft.print("-");
  tft.setCursor(96, 76);
  tft.print("-");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 82);
  tft.println("OKAY = correct wire");
  tft.setCursor(10, 94);
  tft.println("SWAPED = other wire");
  tft.setCursor(10, 106);
  tft.println("BAD = no valid match");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 108);
  tft.println("# = TOGGLE");

  tft.setCursor(10, 104);
  tft.println("* = BACK");
}

void updateAutoCoilAuxStatus() {
  int measured[NUM_WIRES];
  WireStatus status[NUM_WIRES];

  for (uint8_t i = 0; i < NUM_WIRES; i++) {
    measured[i] = readDutyPercent(inPins[i]);
    status[i] = getWireStatus(i, measured[i]);
  }

  AlarmState alarm = getOverallAlarmState(status);
  updateBuzzer(alarm);

  tft.fillRect(48, 56, 80, 35, ST77XX_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(52, 60);
  tft.print(statusText(status[0]));
  tft.setCursor(96, 60);
  tft.print(statusText(status[1]));

  tft.setCursor(52, 76);
  tft.print(statusText(status[2]));
  tft.setCursor(96, 76);
  tft.print(statusText(status[3]));
}

// ---------- wire reading ----------
int readDutyPercent(uint8_t pin) {
  const unsigned long sampleWindowUs = 20000;
  const unsigned int sampleDelayUs = 100;

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
  const int tolerance = 8;

  for (int i = 0; i < NUM_WIRES; i++) {
    if (abs(measuredDuty - dutyPercent[i]) <= tolerance) {
      return i;
    }
  }

  return -1;
}

bool noContactHasSignal(uint8_t pin) {
  const uint8_t sampleCount = 6;
  const uint8_t requiredHits = 4;
  const int signalThreshold = 15;
  const int maxSpread = 18;

  int readings[sampleCount];
  int hits = 0;

  for (uint8_t i = 0; i < sampleCount; i++) {
    readings[i] = readDutyPercent(pin);
    if (readings[i] >= signalThreshold) {
      hits++;
    }
    delay(5);
  }

  int minV = readings[0];
  int maxV = readings[0];

  for (uint8_t i = 1; i < sampleCount; i++) {
    if (readings[i] < minV) minV = readings[i];
    if (readings[i] > maxV) maxV = readings[i];
  }

  if ((maxV - minV) > maxSpread) {
    return false;
  }

  return (hits >= requiredHits);
}

WireStatus getWireStatus(uint8_t wireIndex, int measuredDuty) {
  if (wireIndex == 1 || wireIndex == 3) {
    if (noContactHasSignal(inPins[wireIndex])) {
      return STATUS_BAD;
    }
    return STATUS_OKAY;
  }

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
    case STATUS_SWAPED: return "SWAPED";
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

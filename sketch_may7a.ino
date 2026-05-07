#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// TFT pins
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2

// Buttons
#define BTN_UP      32   // K1
#define BTN_DOWN    33   // K2
#define BTN_ENTER   25   // K3
#define BTN_BACK    26   // K4

// LTE4-2000 auxiliary wiring test pins
// 4 unique pattern inject outputs
const int auxOutPins[4] = {13, 12, 14, 27};
// 4 sense inputs (35, VP/GPIO36, VN/GPIO39 are input-only)
const int auxInPins[4] = {15, 35, 36, 39};

const char* channelNames[4] = {
  "AUX1 NO",
  "AUX1 NC",
  "AUX2 NO",
  "AUX2 NC"
};

// expectedCode[input index] = expected source channel code
const int expectedCode[4] = {1, 2, 3, 4};

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Menu items
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
bool inDetails = false;

void drawMenu();
void showSelected();
void runLTE4AuxWiringTest();
void sendCode(int ch, int pulses);
int detectCodeOnInput(int pin, unsigned long windowMs);
void clearAuxOutputs();

void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  for (int i = 0; i < 4; i++) {
    pinMode(auxOutPins[i], OUTPUT);
    digitalWrite(auxOutPins[i], LOW);
  }

  pinMode(auxInPins[0], INPUT);
  pinMode(auxInPins[1], INPUT);
  pinMode(auxInPins[2], INPUT);
  pinMode(auxInPins[3], INPUT);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setTextWrap(true);
  tft.fillScreen(ST77XX_BLACK);

    tft.setCursor(74, y);
    tft.print("CH");
    tft.print(expectedCode[i]);

void loop() {
  if (!inDetails) {
    if (digitalRead(BTN_DOWN) == LOW) {
      selected++;
      if (selected >= menuSize) selected = 0;
      drawMenu();
      delay(200);
    }

    if (digitalRead(BTN_UP) == LOW) {
      selected--;
      if (selected < 0) selected = menuSize - 1;
      drawMenu();
      delay(200);
    }

    if (digitalRead(BTN_ENTER) == LOW) {
      inDetails = true;
      showSelected();
      delay(200);
    }
  } else {
    if (selected == 0 && digitalRead(BTN_ENTER) == LOW) {
      runLTE4AuxWiringTest();
      delay(250);
    }

    if (digitalRead(BTN_BACK) == LOW) {
      inDetails = false;
      drawMenu();
      delay(200);
    }
  }
}

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
    tft.setCursor(10, y);
    tft.println(menuItems[i]);
  }
}

void showSelected() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println(menuItems[selected]);

  tft.drawLine(10, 34, 150, 34, ST77XX_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  if (selected == 0) {
    tft.setCursor(10, 46);
    tft.println("ENTER: Run aux test");
    tft.setCursor(10, 58);
    tft.println("Detect NO/NC swaps");
    tft.setCursor(10, 70);
    tft.println("Detect cross swaps");
  } else {
    tft.setCursor(10, 52);
    tft.println("No test defined yet");
  }

  tft.setCursor(10, 110);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("K4 = BACK");
}

void sendCode(int ch, int pulses) {
  for (int p = 0; p < pulses; p++) {
    digitalWrite(auxOutPins[ch], HIGH);
    delay(35);
    digitalWrite(auxOutPins[ch], LOW);
    delay(35);
  }
  delay(90);
}

int detectCodeOnInput(int pin, unsigned long windowMs) {
  int edges = 0;
  int last = digitalRead(pin);
  unsigned long t0 = millis();

  while (millis() - t0 < windowMs) {
    int v = digitalRead(pin);
    if (v != last) {
      if (v == HIGH) edges++;
      last = v;
    }
  }
  return edges;
}

void clearAuxOutputs() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(auxOutPins[i], LOW);
  }
}

void runLTE4AuxWiringTest() {
  int seen[4] = {0, 0, 0, 0};

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(8, 6);
  tft.println("LTE4-2000 AUX TEST");
  tft.drawLine(8, 16, 150, 16, ST77XX_WHITE);

  // Run signatures: CH1=1 pulse, CH2=2, CH3=3, CH4=4
  for (int ch = 0; ch < 4; ch++) {
    sendCode(ch, ch + 1);
  }

  // Decode each input line
  for (int i = 0; i < 4; i++) {
    seen[i] = detectCodeOnInput(auxInPins[i], 260);
  }

  clearAuxOutputs();

  // Display mapping and mismatches
  tft.setTextColor(ST77XX_WHITE);
  int y = 22;
  for (int i = 0; i < 4; i++) {
    tft.setCursor(2, y);
    tft.print(i + 1);
    tft.print(": ");
    tft.print(channelNames[i]);
    tft.print(" <- CH");
    tft.println(seen[i]);
    y += 12;
  }

  bool allOk = true;
  for (int i = 0; i < 4; i++) {
    if (seen[i] != expectedCode[i]) {
      allOk = false;
      break;
    }
  }

  tft.setCursor(2, 76);
  if (allOk) {
    tft.setTextColor(ST77XX_GREEN);
    tft.println("WIRING OK");
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.println("SWAP DETECTED");

    int row = 88;
    tft.setTextColor(ST77XX_YELLOW);
    for (int i = 0; i < 4 && row <= 112; i++) {
      if (seen[i] >= 1 && seen[i] <= 4 && seen[i] != expectedCode[i]) {
        tft.setCursor(2, row);
        tft.print(channelNames[i]);
        tft.print(" <-> ");
        tft.println(channelNames[seen[i] - 1]);
        row += 10;
      }
    }
  }

  delay(400);
}

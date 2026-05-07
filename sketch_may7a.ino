#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// TFT pins (keep current screen config)
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2

// Buttons
#define BTN_UP      32   // K1
#define BTN_DOWN    33   // K2
#define BTN_ENTER   25   // K3
#define BTN_BACK    26   // K4

// LTHE4-2000 AUX inputs: NO and NC for each AUX line
const int NUM_AUX = 3;
const int auxNO[NUM_AUX] = {13, 12, 14};
const int auxNC[NUM_AUX] = {27, 35, 36};

const int delayBetween = 500;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

String menuItems[] = {
  "LTHE4-2000",
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
void runLTHE4AuxWiringTest();
char readAuxStatus(int index);
void detectSwaps(char statusArray[]);

void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  for (int i = 0; i < NUM_AUX; i++) {
    pinMode(auxNO[i], INPUT_PULLUP);
    pinMode(auxNC[i], INPUT_PULLUP);
  }

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);

  drawMenu();
}

void loop() {
  if (!inDetails) {
    if (digitalRead(BTN_DOWN) == LOW) {
      selected = (selected + 1) % menuSize;
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
    if (selected == 0) {
      runLTHE4AuxWiringTest();
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
    tft.setTextSize(1);
    tft.setCursor(10, y + 2);
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
    tft.println("Live AUX NO/NC test");
    tft.setCursor(10, 58);
    tft.println("N=NO C=NC O=Open");
    tft.setCursor(10, 70);
    tft.println("F=Fault S=Swap");
  } else {
    tft.setCursor(10, 52);
    tft.println("No test defined yet");
  }

  tft.setCursor(10, 110);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("K4 = BACK");
}

char readAuxStatus(int index) {
  bool noPressed = (digitalRead(auxNO[index]) == LOW);
  bool ncPressed = (digitalRead(auxNC[index]) == LOW);

  if (!noPressed && !ncPressed) return 'O';
  if (noPressed && !ncPressed) return 'N';
  if (!noPressed && ncPressed) return 'C';
  if (noPressed && ncPressed) return 'F';
  return '?';
}

void detectSwaps(char statusArray[]) {
  for (int i = 0; i < NUM_AUX; i++) {
    if (statusArray[i] == '?') statusArray[i] = 'S';
  }
}

void runLTHE4AuxWiringTest() {
  char status[NUM_AUX];

  for (int i = 0; i < NUM_AUX; i++) {
    status[i] = readAuxStatus(i);
  }

  detectSwaps(status);

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(8, 6);
  tft.println("LTHE4-2000 AUX TEST");
  tft.drawLine(8, 16, 150, 16, ST77XX_WHITE);

  tft.setTextColor(ST77XX_WHITE);
  for (int i = 0; i < NUM_AUX; i++) {
    tft.setCursor(10, 24 + (i * 14));
    tft.print("A");
    tft.print(i + 1);
    tft.print(": ");
    tft.print(status[i]);
  }

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 78);
  tft.println("N=NO C=NC O=Open");
  tft.setCursor(10, 90);
  tft.println("F=Fault S=Swap");
  tft.setCursor(10, 110);
  tft.println("K4=BACK");

  delay(delayBetween);
}

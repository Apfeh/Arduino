#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

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

void setup() {

  // Button setup
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  // TFT init
  tft.initR(INITR_BLACKTAB);

  tft.setRotation(0);

  tft.setTextWrap(true);

  tft.fillScreen(ST77XX_BLACK);

  drawMenu();
}

void loop() {

  // ---------------- MENU MODE ----------------
  if(!inDetails) {

    // DOWN
    if(digitalRead(BTN_DOWN) == LOW) {

      selected++;

      if(selected >= menuSize) {
        selected = 0;
      }

      drawMenu();
      delay(200);
    }

    // UP
    if(digitalRead(BTN_UP) == LOW) {

      selected--;

      if(selected < 0) {
        selected = menuSize - 1;
      }

      drawMenu();
      delay(200);
    }

    // ENTER
    if(digitalRead(BTN_ENTER) == LOW) {

      inDetails = true;

      showSelected();

      delay(200);
    }
  }

  // ---------------- DETAILS SCREEN ----------------
  else {

    // BACK
    if(digitalRead(BTN_BACK) == LOW) {

      inDetails = false;

      drawMenu();

      delay(200);
    }
  }
}

// ================= MENU SCREEN =================

void drawMenu() {

  tft.fillScreen(ST77XX_BLACK);

  // Title
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);

  tft.setCursor(10, 5);
  tft.println("CONTACTORS");

  // Menu items
  for(int i = 0; i < menuSize; i++) {

    int y = 30 + (i * 18);

    // Highlight selected item
    if(i == selected) {

      tft.fillRect(0, y - 2, 160, 18, ST77XX_YELLOW);
      tft.setTextColor(ST77XX_BLACK);

    } else {

      tft.setTextColor(ST77XX_WHITE);
    }

    tft.setCursor(10, y);
    tft.println(menuItems[i]);
  }
}

// ================= DETAILS SCREEN =================

void showSelected() {

  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);

  tft.setCursor(10, 20);
  tft.println("SELECTED");

  tft.drawLine(10, 45, 150, 45, ST77XX_WHITE);

  tft.setCursor(10, 60);
  tft.println(menuItems[selected]);

  // Back hint
  tft.setTextSize(1);

  tft.setCursor(10, 110);
  tft.setTextColor(ST77XX_YELLOW);

  tft.println("K4 = BACK");
}

#include <Arduino.h>
#include <EEPROM.h>
#include "ubitx.h"
#include "nano_gui.h"
#include <Adafruit_ILI9341.h>

#include <SPI.h>
#include <avr/pgmspace.h>

#define TFT_CS    10
#define TFT_RS    9
extern Adafruit_ILI9341 tft;

extern struct Point ts_point;


//filled from a test run of calibration routine
int slope_x = 104, slope_y = 137, offset_x = 28, offset_y = 29;

void readTouchCalibration() {
  EEPROM.get(SLOPE_X, slope_x);
  EEPROM.get(SLOPE_Y, slope_y);
  EEPROM.get(OFFSET_X, offset_x);
  EEPROM.get(OFFSET_Y, offset_y);
}

void writeTouchCalibration() {
  EEPROM.put(SLOPE_X, slope_x);
  EEPROM.put(SLOPE_Y, slope_y);
  EEPROM.put(OFFSET_X, offset_x);
  EEPROM.put(OFFSET_Y, offset_y);
}

#define Z_THRESHOLD     400
#define Z_THRESHOLD_INT  75
#define MSEC_THRESHOLD  50
#define SPI_SETTING     SPISettings(2000000, MSBFIRST, SPI_MODE0)

static uint32_t msraw = 0x80000000;
static  int16_t xraw = 0, yraw = 0, zraw = 0;
static uint8_t rotation = 1;

static int16_t touch_besttwoavg( int16_t x , int16_t y , int16_t z ) {
  int16_t da, db, dc;
  int16_t reta = 0;
  if ( x > y ) da = x - y; else da = y - x;
  if ( x > z ) db = x - z; else db = z - x;
  if ( z > y ) dc = z - y; else dc = y - z;

  if ( da <= db && da <= dc ) reta = (x + y) >> 1;
  else if ( db <= da && db <= dc ) reta = (x + z) >> 1;
  else reta = (y + z) >> 1;   //    else if ( dc <= da && dc <= db ) reta = (x + y) >> 1;

  return (reta);
}

static void touch_update() {
  int16_t data[6];

  uint32_t now = millis();
  if (now - msraw < MSEC_THRESHOLD) return;

  SPI.beginTransaction(SPI_SETTING);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(0xB1 /* Z1 */);
  int16_t z1 = SPI.transfer16(0xC1 /* Z2 */) >> 3;
  int z = z1 + 4095;
  int16_t z2 = SPI.transfer16(0x91 /* X */) >> 3;
  z -= z2;
  if (z >= Z_THRESHOLD) {
    SPI.transfer16(0x91 /* X */);  // dummy X measure, 1st is always noisy
    data[0] = SPI.transfer16(0xD1 /* Y */) >> 3;
    data[1] = SPI.transfer16(0x91 /* X */) >> 3; // make 3 x-y measurements
    data[2] = SPI.transfer16(0xD1 /* Y */) >> 3;
    data[3] = SPI.transfer16(0x91 /* X */) >> 3;
  }
  else data[0] = data[1] = data[2] = data[3] = 0; // Compiler warns these values may be used unset on early exit.
  data[4] = SPI.transfer16(0xD0 /* Y */) >> 3;  // Last Y touch power down
  data[5] = SPI.transfer16(0) >> 3;
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
  if (z < 0) z = 0;
  if (z < Z_THRESHOLD) { // if ( !touched ) {
    zraw = 0;
    return;
  }
  zraw = z;

  int16_t x = touch_besttwoavg( data[0], data[2], data[4] );
  int16_t y = touch_besttwoavg( data[1], data[3], data[5] );

  if (z >= Z_THRESHOLD) {
    msraw = now;  // good read completed, set wait
    switch (rotation) {
      case 0:
        xraw = 4095 - y;
        yraw = x;
        break;
      case 1:
        xraw = x;
        yraw = y;
        break;
      case 2:
        xraw = y;
        yraw = 4095 - x;
        break;
      default: // 3
        xraw = 4095 - x;
        yraw = 4095 - y;
    }
  }
}


boolean readTouch() {
  touch_update();
  if (zraw >= Z_THRESHOLD) {
    ts_point.x = xraw;
    ts_point.y = yraw;
    return true;
  }
  return false;
}

void scaleTouch(struct Point *p) {
  
  p->x = ((long)(p->x - offset_x) * 10l) / (long)slope_x;
  p->y = ((long)(p->y - offset_y) * 10l) / (long)slope_y;
}


#if !defined(__INT_MAX__) || (__INT_MAX__ > 0xFFFF)
#define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))
#else
#define pgm_read_pointer(addr) ((void *)pgm_read_word(addr))
#endif

void xpt2046_Init() {
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
}

void displayInit(void) {

  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_RS, OUTPUT);
  xpt2046_Init();
  readTouchCalibration();
}

/**************************************************************************/

void displayRawText(char *text, int x1, int y1, int color, int background, int len) {
  tft.setTextColor(color, background);
  tft.setTextSize(len);
  tft.setCursor(x1 + 4, y1 + 8);
  tft.print(text);
  tft.setTextSize(3);
}

// The generic routine to display one line on the LCD
void displayText(char *text, int x1, int y1, int w, int h, int color, int background, int border, bool rect = 0) {

  tft.fillRect(x1, y1, w , h, background);
  tft.drawRect(x1, y1, w , h, border);
  // tft.drawRoundRect(x1, y1, w, h, 10, border);
  // tft.fillRoundRect(x1+1, y1+1, w-2 ,h-2, 10,background);
  // tft.setCursor(6,18);
  tft.setTextColor(color, background);
  tft.setCursor(x1 + 4, y1 + 8);
  tft.print(text);
  tft.setTextSize(3);

}
void displayMemText(char *text, int x1, int y1, int w, int h, int color, int background, int border, bool rect = 0) {

  tft.fillRect(x1, y1, w , h, background);
  tft.drawRect(x1, y1, w , h, border);
  tft.setTextColor(color, background);
  tft.setCursor(x1 + 4, y1 + 8);
  tft.print(text);
  tft.setTextSize(3);

}

void setupTouch() {
  int x1, y1, x2, y2, x3, y3, x4, y4;
  

  tft.fillScreen(DISPLAY_BLACK);
  tft.setTextColor(DISPLAY_WHITE,DISPLAY_BLACK);
  tft.setTextSize(2);
  tft.setCursor(20,100);
  tft.print((char*)"Click on the x");
  tft.setTextSize(2);
  tft.setCursor(20,140);
  tft.print((char*)"Press TUNE to cancel");

  // TOP-LEFT
  //  displayHline(10,20,20,DISPLAY_WHITE);
  displayRawText((char*)"X", 5, 5,  DISPLAY_WHITE, DISPLAY_BLACK, 1);
  tft.setTextSize(1);
  //  displayVline(20,10,20, DISPLAY_WHITE);
      while (btnDown())delay(100);

  while (!readTouch()) {
    if (btnDown()) {
      while (btnDown())delay(100);
      return;
    }
    delay(100);
  }
  
  while (readTouch())
    delay(100);
  x1 = ts_point.x;
  y1 = ts_point.y;

  displayRawText((char*)" ", 5, 5,  DISPLAY_WHITE, DISPLAY_BLACK, 1);
  tft.setTextSize(1);
  //  displayVline(20,10,20, DISPLAY_BLACK);

  delay(1000);

  //TOP RIGHT
  displayRawText((char*)"X", 300, 5,  DISPLAY_WHITE, DISPLAY_BLACK, 1);
  tft.setTextSize(1);

  while (!readTouch()) {
    if (btnDown()) {
      while (btnDown())delay(100);
      return;
    }
    delay(100);
  }
  while (readTouch())
    delay(100);
  x2 = ts_point.x;
  y2 = ts_point.y;

  displayRawText((char*)" ", 300, 5,  DISPLAY_WHITE, DISPLAY_BLACK, 1);
  tft.setTextSize(1);

  delay(1000);

  //BOTTOM LEFT
  displayRawText((char*)"X", 2, 220,  DISPLAY_WHITE, DISPLAY_BLACK, 1);
  tft.setTextSize(1);

  while (!readTouch()) {
    if (btnDown()) {
      while (btnDown())delay(100);
      return;
    }
    delay(100);
  }
  x3 = ts_point.x;
  y3 = ts_point.y;

  while (readTouch())
    delay(100);
  displayRawText((char*)" ", 2, 220,  DISPLAY_WHITE, DISPLAY_BLACK, 1);
  tft.setTextSize(1);

  delay(1000);

  //BOTTOM RIGHT
  displayRawText((char*)"X", 300, 220,  DISPLAY_WHITE, DISPLAY_BLACK, 1);
  tft.setTextSize(1);

  while (!readTouch()) {
    if (btnDown()) {
      while (btnDown())delay(100);
      return;
    }
    delay(100);
  }
  x4 = ts_point.x;
  y4 = ts_point.y;


  //  displayHline(290,220,20,DISPLAY_BLACK);
  displayRawText((char*)" ", 300, 220,  DISPLAY_WHITE, DISPLAY_BLACK, 1);
  //  displayVline(300,210,20, DISPLAY_BLACK);

  // we average two readings and divide them by half and store them as scaled integers 10 times their actual, fractional value
  //the x points are located at 20 and 300 on x axis, hence, the delta x is 280, we take 28 instead, to preserve fractional value,
  //there are two readings (x1,x2) and (x3, x4). Hence, we have to divide by 28 * 2 = 56
  slope_x = ((x4 - x3) + (x2 - x1)) / 56;
  //the y points are located at 20 and 220 on the y axis, hence, the delta is 200. we take it as 20 instead, to preserve the fraction value
  //there are two readings (y1, y2) and (y3, y4). Hence we have to divide by 20 * 2 = 40
  slope_y = ((y3 - y1) + (y4 - y2)) / 40;

  //x1, y1 is at 20 pixels
  offset_x = x1 + -((20 * slope_x) / 10);
  offset_y = y1 + -((20 * slope_y) / 10);

  writeTouchCalibration();
}

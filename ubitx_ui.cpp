#include <Arduino.h>
#include <EEPROM.h>
#include "ubitx.h"
#include "nano_gui.h"
#include <TimeLib.h>
#include <Adafruit_ILI9341.h>

#ifdef KEYBOARD_INCLUDED
#include <PS2Keyboard.h>
extern PS2Keyboard keyboard;
#endif

extern void displayMemText(char *text, int x1, int y1, int w, int h, int color, int background, int border, bool rect = 0);
extern int Region;
extern int KeyIndex;
extern struct Point ts_point;
extern bool RTC_Clock;
extern void Set_Mode ( void );
extern volatile int TimeToSaveFrequency;

extern Adafruit_ILI9341 tft;
extern void doTuning( void );
extern bool TXON;

extern int Freq_Index;
extern unsigned long Band_Low_Limits[3][8];
extern unsigned long Band_High_Limits[3][8];
extern char Version_Text[21];
extern int Smtr_Constant;
extern volatile int cwMode;

extern volatile bool TX_Inhibit;

void   display_Menu_Bar(void);

int Active_Bar_Menu = 1;
#define MAXMENUS 3


bool Smtr_Active = false;


bool Lck_Active = false;
bool Spd_Active = false;
int Current_Mode = 0;

volatile int SmeterReadValue, LastSmeter, SMeter_Value;


#define BUTTON_SELECTED 1

struct Button {
  int x, y, w, h;
  char *text;
};


int Encoder_Button_Advance[19] = {8, 9, 10, 11, 12, 13, 14, 0, 1, 2, 3, 4, 5, 6, 7, 15, 16, 17};

#define MAX_KEYS 15
const struct Button keypad[MAX_KEYS] PROGMEM = {
  {0, 80, 60, 36,  (char*)"1"},
  {64, 80, 60, 36, (char*)"2"},
  {128, 80, 60, 36, (char*)"3"},
  {192, 80, 60, 36,  (char*)""},
  {256, 80, 60, 36,  (char*)"OK"},

  {0, 120, 60, 36,  (char*)"4"},
  {64, 120, 60, 36,  (char*)"5"},
  {128, 120, 60, 36,  (char*)"6"},
  {192, 120, 60, 36,  (char*)"0"},
  {256, 120, 60, 36,  (char*)"<-"},

  {0, 160, 60, 36,  (char*)"7"},
  {64, 160, 60, 36, (char*)"8"},
  {128, 160, 60, 36, (char*)"9"},
  {192, 160, 60, 36,  (char*)""},
  {256, 160, 60, 36,  (char*)"Can"},
};


struct MemButton {
  int x, y, w, h;
  char *entry;
};

#define MAX_MEMORIES 10
struct MemButton MemButtons[MAX_MEMORIES] = {
  {2, 52, 150, 26,    (char*)"S U 14255.00"},
  {2, 79, 150, 26,    (char*)"C U  7040.00"},
  {2, 106, 150, 26,   (char*)"S L  7255.00"},
  {2, 133, 150, 26,   (char*)"C L  7060.00"},
  {2, 160, 150, 26,   (char*)"S L  7255.00"},
  {165, 52, 150, 26,  (char*)"S L  7255.00"},
  {165, 79, 150, 26,  (char*)"S L  7255.00"},
  {165, 106, 150, 26, (char*)"S L  7255.00"},
  {165, 133, 150, 26, (char*)"S U 28455.00"},
  {165, 160, 150, 26, (char*)"S L  7255.00"},

};


struct TimeButton {
  int x, y, w, h;
};

#define MAX_TIMES 7
struct TimeButton Time_Entrie[MAX_TIMES] = {
  {2, 120, 42, 36 },
  {48, 120, 42, 36 },
  {110, 120, 42, 36 },
  {156, 120, 42, 36 },
  {220, 120, 42, 36 },
  {266, 120, 42, 36 },
  {105, 160, 100, 36 },

};

void Print_Debug( char *text, int val) {
  sprintf(c, "%s %0X", text, val);
  displayText(c, 2, 158, 242, 38, DISPLAY_CYAN, DISPLAY_BLACK, DISPLAY_WHITE, true);
}


int  VfoaBand = 0;
int  VfobBand = 0;
int  Current_Band = 0;

int BandModeVFOa[8];
int BandModeVFOb[8];

unsigned long BandFreqVFOa[8];

unsigned long BandFreqVFOb[8];

#ifdef KEYBOARD_INCLUDED

void Clear_Keyboard( void ) {


  while (keyboard.available()) {
    keyboard.read();
    KeyIndex = 0;
  }

}
#endif

void drawSmeter(void) {


  tft.drawRect(130, 20, 186, 10, DISPLAY_WHITE);
  tft.setTextSize(1);
  tft.setCursor(130, 10);
  tft.setTextColor(DISPLAY_WHITE, DISPLAY_BLUE);
  tft.print("s1   3  5  7  9");
  tft.setTextSize(1);
  tft.setCursor(256, 10);
  tft.print("20 30 40");
  tft.setTextSize(3);
}



void initBandData( void) {
  int i;

  // get band frequencies

  EEPROM.get( BANDFREQVFOa_80, BandFreqVFOa[0]);
  if (( 3500000L > BandFreqVFOa[0]) || (BandFreqVFOa[0] > 4000000L)) BandFreqVFOa[0] = 3560000L;
  EEPROM.get( BANDFREQVFOa_40, BandFreqVFOa[1]);
  if (( 7000000L > BandFreqVFOa[1]) || (BandFreqVFOa[1] > 7300000L))  BandFreqVFOa[1] = 7030000L;
  EEPROM.get( BANDFREQVFOa_30, BandFreqVFOa[2]);
  if ((10100000L > BandFreqVFOa[2]) || (BandFreqVFOa[2] > 10150000L))BandFreqVFOa[2] = 10106000L;
  EEPROM.get( BANDFREQVFOa_20, BandFreqVFOa[3]);
  if ((14000000L > BandFreqVFOa[3]) || (BandFreqVFOa[3] > 14350000L))BandFreqVFOa[3] = 14060000L;
  EEPROM.get( BANDFREQVFOa_17, BandFreqVFOa[4]);
  if ((18068000L > BandFreqVFOa[4]) || (BandFreqVFOa[4] > 18168000L))BandFreqVFOa[4] = 18096000L;
  EEPROM.get( BANDFREQVFOa_15, BandFreqVFOa[5]);
  if ((21000000L > BandFreqVFOa[5]) || (BandFreqVFOa[5] > 21450000L))BandFreqVFOa[5] = 21060000L;
  EEPROM.get( BANDFREQVFOa_10, BandFreqVFOa[6]);
  if ((28000000L > BandFreqVFOa[6]) || (BandFreqVFOa[6] > 29700000L))BandFreqVFOa[6] = 28060000L;
  EEPROM.get( BANDFREQVFOa_GEN, BandFreqVFOa[7]);
  if ((1000000L > BandFreqVFOa[7]) || (BandFreqVFOa[7] > 40000000L))BandFreqVFOa[7] = 2000000L;



  EEPROM.get( BANDFREQVFOb_80, BandFreqVFOb[0]);
  if ((3500000L > BandFreqVFOb[0]) || (BandFreqVFOb[0] > 4000000L))BandFreqVFOb[0] = 3560000L;
  EEPROM.get( BANDFREQVFOb_40, BandFreqVFOb[1]);
  if ((7000000L > BandFreqVFOb[1]) || (BandFreqVFOb[1] > 7300000L))BandFreqVFOb[1] = 7030000L;
  EEPROM.get( BANDFREQVFOb_30, BandFreqVFOb[2]);
  if ((10100000L > BandFreqVFOb[2]) || (BandFreqVFOb[2] > 10150000L))BandFreqVFOb[2] = 10106000L;
  EEPROM.get( BANDFREQVFOb_20, BandFreqVFOb[3]);
  if ((14000000L > BandFreqVFOb[3]) || (BandFreqVFOb[3] > 14350000L))BandFreqVFOb[3] = 14060000L;
  EEPROM.get( BANDFREQVFOb_17, BandFreqVFOb[4]);
  if ((18068000L > BandFreqVFOb[4]) || (BandFreqVFOb[4] > 18168000L))BandFreqVFOb[4] = 18096000L;
  EEPROM.get( BANDFREQVFOb_15, BandFreqVFOb[5]);
  if ((21000000L > BandFreqVFOb[5]) || (BandFreqVFOb[5] > 21450000L))BandFreqVFOb[5] = 21060000L;
  EEPROM.get( BANDFREQVFOb_10, BandFreqVFOb[6]);
  if ((28000000L > BandFreqVFOb[6]) || (BandFreqVFOb[6] > 29700000L))BandFreqVFOb[6] = 28060000L;
  EEPROM.get( BANDFREQVFOb_GEN, BandFreqVFOb[7]);
  if ((100000L > BandFreqVFOb[7]) || (BandFreqVFOb[7] > 40000000L))BandFreqVFOb[7] = 2000000L;

  // get band modes
  for ( i = 0; i < 8; i++) {
    // VFO A
    EEPROM.get( BANDMODEVFOa_80 + (i * 4), BandModeVFOa[i]);
    if ((BandModeVFOa[i] < 0 ) || (BandModeVFOa[i] > 4)) {
      BandModeVFOa[i] = 2;
      EEPROM.get( BANDMODEVFOa_80 + (i * 4), BandModeVFOa[i]);
    }
    // VFO B
    EEPROM.get( BANDMODEVFOb_80 + (i * 4), BandModeVFOb[i]);
    if ((BandModeVFOb[i] < 0 ) || (BandModeVFOb[i] > 4)) {
      BandModeVFOb[i] = 2;
      EEPROM.get( BANDMODEVFOb_80 + (i * 4), BandModeVFOb[i]);
    }
  }
  // set bands
  EEPROM.get( LASTBANDVFOA, VfoaBand);
  if ( (0 >= VfoaBand) || (VfoaBand > 7) ) VfoaBand = 0;

  EEPROM.get( LASTBANDVFOB, VfobBand);
  if ( (0 >= VfobBand) && (VfobBand <= 7) ) VfobBand = 0;


}


int last_band = 0;
bool marker_on = false;




/*
   This formats the frequency given in f
*/
void formatFreq(long f, char *buff) {

  memset(buff, 0, 10);
  memset(b, 0, sizeof(b));

  ultoa(f, b, DEC);

  if (strlen(b) < 6) {
    strcpy(&buff[0], "   ");
    strncat(buff, &b[0], 2);
    strcat(buff, ".");
    strncat(buff, &b[2], 2);
  } else if (strlen(b) <= 6) {
    strcpy(&buff[0], "  ");
    strncat(buff, &b[0], 3);
    strcat(buff, ".");
    strncat(buff, &b[3], 2);
  } else if (strlen(b) <= 7) {
    strcpy(&buff[0], " ");
    strncat(buff, &b[0], 1);
    strncat(buff, &b[1], 3);
    strcat(buff, ".");
    strncat(buff, &b[4], 2);
  } else {
    strncpy(buff, b, 5);
    // strncat(buff, &b[2], 3);
    strcat(buff, ".");
    strncat(buff, &b[5], 2);
  }
}


/** A generic control to read variable values
*/


void printCarrierFreq(unsigned long freq) {

  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ultoa(freq, b, DEC);

  strcpy(c, " ");
  strncat(c, b, 2);
  strcat(c, ".");
  strncat(c, &b[2], 3);
  strcat(c, ".");
  strncat(c, &b[5], 1);
  displayText(c, 100, 126, 180, 38, DISPLAY_CYAN, DISPLAY_BLACK, DISPLAY_WHITE, true);
  tft.drawRoundRect( 100, 126, 180, 38, 10, DISPLAY_WHITE);
}

void displayDialog(char *title, char *instructions) {
  tft.fillScreen(DISPLAY_BLACK);
  tft.drawRect(10, 10, 300, 220, DISPLAY_WHITE);
  tft.drawRect(12, 12, 296, 216, DISPLAY_WHITE);
  displayRawText(title, 20, 20, DISPLAY_CYAN, DISPLAY_BLUE, 2);
  displayText(title, 20, 20, 100, 30, DISPLAY_CYAN, DISPLAY_NAVY, DISPLAY_NAVY, true);
  displayRawText(instructions, 20, 200, DISPLAY_CYAN, DISPLAY_BLUE, 2);
  displayText(instructions, 20, 200, 100, 30, DISPLAY_CYAN, DISPLAY_NAVY, DISPLAY_NAVY, true);
}

int freqindex[6] = { 58, 94, 130, 166, 238, 274 };

#define VFODISPLAYACTIVEY 100
#define VFODISPLAYNOTACTIVEY 50

void  Display_Freq_Underline( void ) {

  tft.fillRect(20, 148, 280, 8, DISPLAY_BLUE);
  tft.fillRect(freqindex[Freq_Index], 150, 26, 6, DISPLAY_YELLOW);
  EEPROM.put(SELECTED_FREQ_INDEX, Freq_Index);
}

char  ModeText[5][4] = { "USB", "LSB", " CW", "CWR", "CWP" };

void  display_Mode( void ) {

  tft.setTextSize(3);
  if ( (0 <= Current_Mode) && (Current_Mode < 5))
    displayText((char*)ModeText[Current_Mode], 180, 38, 44, 33, DISPLAY_GREEN, DISPLAY_BLUE, DISPLAY_BLUE, false);
  else
    displayText((char*)"ERR", 180, 38, 44, 33, DISPLAY_GREEN, DISPLAY_BLUE, DISPLAY_BLUE, false);
}

char  BandText[8][4] = { "80m", "40m", "30m", "20m", "17m", "15m", "10m", "G/C" };

int last_displayed_band = -1;

void  displayBand(void) {

  if ( last_displayed_band != Current_Band) {
    tft.setTextSize(3);
    displayText((char*)BandText[Current_Band], 116, 38, 44, 33, DISPLAY_GREEN, DISPLAY_BLUE, DISPLAY_BLUE, false);
    last_displayed_band = Current_Band;
  }
}


void displayVFO(int vfo) {

  tft.setFont();

  if ( vfo == vfoActive ) {
    // process as selected display
    tft.setTextSize(3);
    //tft.setCursor(116, VFODISPLAYACTIVEY);
    tft.setCursor(2, VFODISPLAYACTIVEY);
    //  tft.setCursor(50, VFODISPLAYACTIVEY);
    if (vfo == VFO_A) {
      if (splitOn) {
        strcpy(c, "R");
      } else strcpy(c, "A");
    } else {
      if (splitOn) {
        strcpy(c, "R");
      } else strcpy(c, "B");
    }
    tft.setTextColor(DISPLAY_WHITE, DISPLAY_BLUE);
    tft.print(c);
    tft.setCursor(2, VFODISPLAYACTIVEY + 24);
    if (Lck_Active == true )tft.print("L");
    else tft.print(" ");
    if (Lck_Active == true ) {
      tft.setTextColor(DISPLAY_RED, DISPLAY_BLUE);
    } else {
      tft.setTextColor(DISPLAY_WHITE, DISPLAY_BLUE);
    }
    tft.setTextSize(6);
    tft.setCursor(20, VFODISPLAYACTIVEY);
    formatFreq(frequency, c);
    tft.print(c);
    // displayBand();
    // process as deselected display
  } else {
    if (vfo == VFO_B) {
      tft.setTextSize(2);
      tft.setCursor(2, VFODISPLAYNOTACTIVEY);
      if (splitOn) {
        strcpy(c, "T");
      } else strcpy(c, "B");
      formatFreq(BandFreqVFOb[VfobBand], c + 1);
      tft.setTextColor(DISPLAY_GREEN, DISPLAY_BLUE);
      tft.print(c);
    } else {
      tft.setTextSize(2);
      tft.setCursor(2, VFODISPLAYNOTACTIVEY);
      if (splitOn) {
        strcpy(c, "T");
      } else strcpy(c, "A");
      formatFreq(BandFreqVFOa[VfoaBand], c + 1);
      tft.setTextColor(DISPLAY_GREEN, DISPLAY_BLUE);
      tft.print(c);
    }
  }
  //display_Mode();
  tft.setTextSize(3);

  Display_Freq_Underline();


}




void btnDraw(struct Button * b) {



  displayText(b->text, b->x, b->y, b->w, b->h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_WHITE, false);

}







void displayRIT() {
  tft.fillRect(0, 68, 130, 32, DISPLAY_BLUE);
  if (ritOn) {
    strcpy(c, "Tx:");
    formatFreq(ritTxFrequency, c + 2);
    displayRawText(c, 0, 65,  DISPLAY_WHITE, DISPLAY_BLUE, 2);
  } else {
    if ( VFO_A == vfoActive ) displayVFO(VFO_B);
    else displayVFO(VFO_A);
  }
}

void enterFreq() {
  unsigned long f;

  // clear underline from band buttons
  tft.fillRect(0, 32, 318, 48, DISPLAY_BLUE);
  tft.fillRect(0, 78, 318, 122, DISPLAY_ORANGE);

  //force the display to refresh everything
  //display all the buttons

  for (int i = 0; i < MAX_KEYS; i++) {
    struct Button b;
    memcpy_P(&b, keypad + i, sizeof(struct Button));
    btnDraw(&b);
  }

  int cursor_pos = 0;
  memset(c, 0, sizeof(c));

  while (1) {
    delay(100);
    if (!readTouch()) continue;
    scaleTouch(&ts_point);

    for (int i = 0; i < MAX_KEYS; i++) {
      struct Button b;
      memcpy_P(&b, keypad + i, sizeof(struct Button));

      int x2 = b.x + b.w;
      int y2 = b.y + b.h;

      if (b.x < ts_point.x && ts_point.x < x2 &&
          b.y < ts_point.y && ts_point.y < y2) {
        if (!strcmp(b.text, "OK")) {
          f = atol(c);
          f = f * 1000L;
          if ( f <= Band_High_Limits[Region - 1][Current_Band] && f >= Band_Low_Limits[Region - 1][Current_Band]) {

            // determine band that entered freq must be placed
            if (vfoActive == VFO_A) {
              EEPROM.put(BANDFREQVFOa_80 + (4 * VfoaBand), f);
              VfoaBand = Current_Band;
              BandFreqVFOa[VfoaBand] = f;
              setFrequency(BandFreqVFOa[Current_Band]);
            } else {
              EEPROM.put(BANDFREQVFOb_80 + (4 * VfobBand), f);
              VfobBand = Current_Band;
              BandFreqVFOb[VfobBand] = f;
              setFrequency(BandFreqVFOb[Current_Band]);
            }
          }
          guiUpdate();
          return;
        }
        else if (!strcmp(b.text, "<-")) {
          c[cursor_pos] = 0;
          if (cursor_pos > 0)
            cursor_pos--;
          c[cursor_pos] = 0;
        }
        else if (!strcmp(b.text, "Can")) {
          guiUpdate();
          return;
        }
        else if ('0' <= b.text[0] && b.text[0] <= '9') {
          c[cursor_pos++] = b.text[0];
          c[cursor_pos] = 0;
        }
      }
    } // end of the button scanning loop
    strcpy(b, c);
    strcat(b, " KHz");
    displayText(b, 0, 42, 320, 30, DISPLAY_WHITE, DISPLAY_NAVY, DISPLAY_NAVY, true);
    delay(300);
    while (readTouch()) active_delay(100);
  } // end of event loop : while(1)
  while (readTouch()) active_delay(100);

}




void Display_S_Meter( void ) {
  int Normal_Smeter_Length;
  int Red_Smeter_Length;
  int  level;

  SmeterReadValue = analogRead(SMETERREADPIN);   // read value of signal 0 - 1023
  level = SmeterReadValue;  // kill time
  SmeterReadValue = analogRead(SMETERREADPIN);   // read value of signal 0 - 1023

  level = SmeterReadValue / Smtr_Constant;

  if (level > 40) level = 47;
  else if (level > 30) level = 40;
  else if (level > 20 ) level = 30;
  else if (level > 10 ) level = 20;
  else if (level > 7  ) level = 9;
  else if (level > 5  ) level = 7;
  else if (level > 3  ) level = 5;
  else if (level > 1  ) level = 3;
  else if (level > 0  ) level = 1;
  else level = 0;

  if (!cwMode)noInterrupts();
  if (level != LastSmeter ) {
    tft.fillRect(130, 22, 184, 6, ILI9341_WHITE);
    Normal_Smeter_Length = level * 10;
    if ( level <= 9 ) {
      Normal_Smeter_Length = level * 10;
      tft.fillRect(130, 22, Normal_Smeter_Length, 6, ILI9341_GREEN);
    } else {
      tft.fillRect(130, 22, 100, 6, ILI9341_GREEN);
      // red zone   40   130  -> 40y = 130x    y= 130/40
      Red_Smeter_Length = (level) * 2;
      if ( Red_Smeter_Length > 130 )Red_Smeter_Length = 130;
      tft.fillRect(220, 22, Red_Smeter_Length, 6, ILI9341_RED);
    }
    LastSmeter = level;
  }
  if (!cwMode)interrupts();
}




void drawTxON() {

  tft.setTextSize(2);
  //tft.fillRect(290, 160, 24, 33,DISPLAY_RED);
  displayText((char*)"Tx", 100, 2, 30, 30, DISPLAY_YELLOW, DISPLAY_RED, DISPLAY_YELLOW, true);

}

void drawTxOFF() {

  tft.fillRect(100, 2, 30, 30, DISPLAY_BLUE);

}


void guiUpdate() {

  // use the current frequency as the VFO frequency for the active VFO
  tft.fillScreen(DISPLAY_BLUE);
  displayRIT();
  sprintf(c, "%02d", wpm);
  tft.setTextSize(3);
  displayText(c, 240, 38, 44, 33, DISPLAY_GREEN, DISPLAY_BLUE, DISPLAY_BLUE, false);
  tft.setTextSize(2);
  tft.setCursor(280, 52);
  tft.print("wpm");
  tft.setTextSize(1);
  tft.setTextColor(DISPLAY_GREEN, DISPLAY_BLUE);
  //tft.setCursor(300, 230);
  tft.setCursor(1, 1);
  tft.print(Version_Text);
  tft.setTextSize(3);
  if ( Smtr_Active )drawSmeter();
  display_Menu_Bar();
  last_displayed_band = -1;
  displayBand();
  display_Mode();
  displayVFO(VFO_A);
  displayVFO(VFO_B);

}



// this builds up the top line of the display with frequency and mode
void updateDisplay() {

  displayVFO(vfoActive);

}

int enc_prev_state = 3;

/**
   The A7 And A6 are purely analog lines on the Arduino Nano
   These need to be pulled up externally using two 10 K resistors

   There are excellent pages on the Internet about how these encoders work
   and how they should be used. We have elected to use the simplest way
   to use these encoders without the complexity of interrupts etc to
   keep it understandable.

   The enc_state returns a two-bit number such that each bit reflects the current
   value of each of the two phases of the encoder

   The enc_read returns the number of net pulses counted over 50 msecs.
   If the puluses are -ve, they were anti-clockwise, if they are +ve, the
   were in the clockwise directions. Higher the pulses, greater the speed
   at which the enccoder was spun
*/


int enc_read(void) {
  int result = 0;
  byte newState = 0;
  byte nowState;
  int enc_speed = 0;

  unsigned long stop_by = millis() + 30;

  while (millis() < stop_by) { // check if the previous state was stable
    newState = (digitalRead(ENC_A) == 1 ? 1 : 0) + (digitalRead(ENC_B) == 1 ? 2 : 0); // Get current state

    //    if (newState != enc_prev_state)
    //      active_delay(20);
    nowState = (digitalRead(ENC_A) == 1 ? 1 : 0) + (digitalRead(ENC_B) == 1 ? 2 : 0);
    if (nowState != newState || newState == enc_prev_state)
      continue;
    //these transitions point to the encoder being rotated anti-clockwise
    if ((enc_prev_state == 0 && newState == 2) ||
        (enc_prev_state == 2 && newState == 3) ||
        (enc_prev_state == 3 && newState == 1) ||
        (enc_prev_state == 1 && newState == 0)) {
      result--;
    }
    //these transitions point o the enccoder being rotated clockwise
    if ((enc_prev_state == 0 && newState == 1) ||
        (enc_prev_state == 1 && newState == 3) ||
        (enc_prev_state == 3 && newState == 2) ||
        (enc_prev_state == 2 && newState == 0)) {
      result++;
    }
    enc_prev_state = newState; // Record state for next pulse interpretation
    enc_speed++;
  }
  return (result);
}



void ritToggle(void) {
  if ( splitOn == 0 ) {
    if (ritOn == 0) {
      ritEnable(frequency);
      tft.setTextColor(DISPLAY_GREEN);
    } else {
      ritDisable();
      tft.setTextColor(DISPLAY_WHITE);
    }
    tft.setTextSize(2);
    tft.setCursor(5, 208);
    if ( Active_Bar_Menu == 2 )tft.print((char*)" RIT ");
    displayRIT();

  }
}


void splitToggle(void) {

  if (ritOn == 0) {
    tft.setTextSize(2);
    tft.setCursor(65, 208);
    if (splitOn) {
      tft.setTextColor(DISPLAY_WHITE);
      splitOn = 0;
    } else {
      splitOn = 1;
      tft.setTextColor(DISPLAY_GREEN);
    }
    if ( Active_Bar_Menu == 2 )tft.print((char*)"Split");

    //disable rit as well
    ritDisable();

    displayRIT();
    displayVFO(VFO_A);
    displayVFO(VFO_B);
  }
}





void redrawVFOs() {

  displayVFO(VFO_A);
  displayVFO(VFO_B);

}


void switchBand(int band, bool Display) {

  Current_Band = band;
  if (vfoActive == VFO_A) {
    EEPROM.put(BANDFREQVFOa_80 + (4 * VfoaBand), frequency);
    BandFreqVFOa[VfoaBand] = frequency;
    EEPROM.put(BANDMODEVFOa_80 + (4 * VfoaBand), Current_Mode);
    //------
    VfoaBand = band;
    EEPROM.put( LASTBANDVFOA, VfoaBand);
    setFrequency(BandFreqVFOa[VfoaBand]);
    EEPROM.get(BANDMODEVFOa_80 + (4 * VfoaBand), Current_Mode);
  } else {
    EEPROM.put(BANDFREQVFOb_80 + (4 * VfobBand), frequency);
    BandFreqVFOb[VfobBand] = frequency;
    EEPROM.put(BANDMODEVFOb_80 + (4 * VfobBand), Current_Mode);
    //------
    VfobBand = band;
    EEPROM.put( LASTBANDVFOB, VfobBand);
    setFrequency(BandFreqVFOb[VfobBand]);
    EEPROM.get(BANDMODEVFOb_80 + (4 * VfobBand), Current_Mode);
  }
  Set_Mode();
  if ( Display ) {
    displayRIT();
    displayVFO(VFO_A);
    displayVFO(VFO_B);
    displayBand();
    display_Mode();
  }

}



void CopyAtoB(void) {

  VfobBand = VfoaBand;
  BandFreqVFOb[VfobBand] = BandFreqVFOa[VfoaBand];
  BandModeVFOb[VfobBand] = BandModeVFOa[VfoaBand];
  Current_Mode = BandModeVFOa[VfoaBand];
  EEPROM.put(BANDFREQVFOb_80 + (4 * VfobBand), BandFreqVFOa[VfoaBand]);
  EEPROM.put(BANDMODEVFOb_80 + (4 * VfobBand), BandModeVFOa[VfoaBand]);
  frequency = BandFreqVFOa[VfoaBand];
  setFrequency(frequency);
  switchBand(VfoaBand, false);
  switchBand(VfobBand, true);

}


void setCwTone() {
  int knob = 0;

  sprintf(c, "%04d", (int)sideTone);
  tft.setTextSize(2);
  displayText(c, 160, 52, 58, 24, DISPLAY_WHITE, DISPLAY_BLACK, DISPLAY_BLUE, true);

  // analogWrite(CW_TONE, volumne);
  tone(CW_TONE, sideTone);

  //disable all clock 1 and clock 2
  while (!readTouch() ) {
    knob = enc_read();

    if (knob > 0 && sideTone < 2000)
      sideTone += 10;
    else if (knob < 0 && sideTone > 100 )
      sideTone -= 10;
    else
      continue; //don't update the frequency or the display

    tone(CW_TONE, sideTone);
    sprintf(c, "%04d", (int)sideTone);
    tft.setTextSize(2);
    displayText(c, 160, 52, 58, 24, DISPLAY_WHITE, DISPLAY_BLACK, DISPLAY_BLUE, true);
    active_delay(20);
  }
  //noTone(CW_TONE);
  //save the setting
  EEPROM.put(CW_SIDETONE, sideTone);
  sprintf(c, "%04d", (int)sideTone);
  tft.setTextSize(2);
  displayText(c, 160, 52, 58, 24, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);

  // while (btnDown())delay(20);
  while (readTouch())active_delay(100);
}



void LckToggle( void ) {

  tft.setTextSize(2);
  tft.setCursor(257, 208);
  if (Lck_Active == true ) {
    Lck_Active = false;
    tft.setTextColor(DISPLAY_WHITE);
  }
  else {
    Lck_Active = true;
    tft.setTextColor(DISPLAY_GREEN);
  }
  if ( Active_Bar_Menu == 2 )tft.print((char*)"Lock");

}


//    ------------------------------- process memories ------------------

void displayMemButton(char *text, int x1, int y1, int w, int h, int color, int background, int border, bool rect = 0) {

  tft.fillRect(x1, y1, w , h, background);
  tft.drawRect(x1, y1, w , h, border);
  tft.setTextColor(color, background);
  tft.setCursor(x1 + 5, y1 + 4);
  tft.print(text);
  tft.setTextSize(3);

}

char modetext[4][5] = {"S U ", "S L ", "C U ", "C L "};


void Select_Memory_Channel(int ibtn, int color) {
  struct MemButton b;

  memcpy_P(&b, MemButtons + ibtn, sizeof(struct MemButton));
  tft.drawRect(b.x, b.y, b.w, b.h, color);
}

void Display_Memory_Channel( int bank, int channel ) {
  unsigned long entryvalue, entryfreq;
  int entrymode;
  char entrytext[13];


  struct MemButton b;
  memcpy_P(&b, MemButtons + channel, sizeof(struct MemButton));
  tft.setTextSize(2);
  EEPROM.get(MEMBANK01_0 + (channel * 4) + (bank * 40), entryvalue);
  if ( entryvalue > 1000000000L )strcpy(b.entry, "blank");
  else {
    entryfreq = (entryvalue / 10L) * 10L;
    entrymode = entryvalue - entryfreq;
    entryfreq = entryfreq / 100L;
    sprintf(entrytext, "%lu", entryfreq);
    strcpy(b.entry, modetext[entrymode]);
    if ( strlen(entrytext) == 5 ) {
      strcat(b.entry, "  ");
      strncat(b.entry, entrytext, 3);
      strcat(b.entry, ".");
      strncat(b.entry, &entrytext[3], 2);
    } else if ( strlen(entrytext) == 6 ) {
      strcat(b.entry, " ");
      strncat(b.entry, entrytext, 4);
      strcat(b.entry, ".");
      strncat(b.entry, &entrytext[4], 2);
    } else {
      strncat(b.entry, entrytext, 5);
      strcat(b.entry, ".");
      strncat(b.entry, &entrytext[5], 2);
    }
  }
  displayMemText(b.entry, b.x, b.y, b.w, b.h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_DARKGREY, false);

}


void Display_Bank( int bank ) {

  tft.fillRect(0, 46, 318, 224, DISPLAY_BLUE);

  for (int i = 0; i < MAX_MEMORIES; i++) {
    Display_Memory_Channel( 0, i );
  }
  tft.setTextSize(2);
  displayMemButton((char*)"M>VFO", 2, 210, 70, 22, DISPLAY_BLUE, DISPLAY_WHITE, DISPLAY_RED, true);
  tft.setTextSize(2);
  displayMemButton((char*)"VFO>M", 80, 210, 70, 22, DISPLAY_BLUE, DISPLAY_WHITE, DISPLAY_RED, true);
  tft.setTextSize(2);
  displayMemButton((char*)"Exit", 244, 210, 70, 22, DISPLAY_WHITE, DISPLAY_RED, DISPLAY_WHITE, true);
  tft.setTextSize(2);
  // future update
  //tft.setTextSize(2);
  //displayMemButton((char*)"Next", 166, 210, 70, 22, DISPLAY_BLUE, DISPLAY_WHITE, DISPLAY_RED, true);
  //tft.setCursor(246, 192);
  //tft.setTextSize(2);
  //tft.setTextColor(DISPLAY_WHITE);
  //tft.print((char*)"Bank 1");
  tft.setTextSize(2);
  displayMemButton((char*)"Blank", 166, 210, 70, 22, DISPLAY_BLUE, DISPLAY_WHITE, DISPLAY_RED, true);

}


void Process_Load ( int bank, int Last_Memory_Selected ) {
  int mode, modifier, band;
  unsigned long New_Frequency, Temp_Frequency;


  EEPROM.get(MEMBANK01_0 + (Last_Memory_Selected * 4) + (bank * 40), New_Frequency);

  // do not process blank memory channel
  if ( New_Frequency < 500000000 ) {
    Temp_Frequency = (New_Frequency / 100L) * 100L;
    modifier = New_Frequency - Temp_Frequency;
    band = modifier / 10;
    switchBand(band, true);
    frequency = Temp_Frequency / 10L;
    mode = modifier - (band * 10);
    //  USB = 0, LSB = 1, CW = 2, CWR = 3
    if ( mode == 0 ) {
      isUSB = 1;
      cwMode = 0;
    }
    else if ( mode == 1 ) {
      isUSB = 0;
      cwMode = 0;
    }
    else if ( mode == 2 ) {
      isUSB = 1;
      cwMode = 1;
    }
    else if ( mode == 3 ) {
      isUSB = 0;
      cwMode = 1;
    }
    Current_Mode = mode;
    setFrequency(frequency);
    if (vfoActive == VFO_A)        EEPROM.put(BANDFREQVFOa_80 + (4 * VfoaBand), frequency);
    else EEPROM.put(BANDFREQVFOb_80 + (4 * VfobBand), frequency);
    redrawVFOs();
    Display_Bank(bank);
  } else Display_Memory_Channel( bank, Last_Memory_Selected );
}

void Process_Store ( int bank, int Last_Memory_Selected , unsigned long Freq ) {
  int mode, modifier, band;
  unsigned long New_Memory_Frequency;

  // modifier = band*10+mode
  //  USB = 0, LSB = 1, CW = 2, CWR = 3
  mode = -1;
  if ( isUSB == 1 && cwMode == 0 )mode = 0;
  else if ( isUSB == 0 && cwMode == 0 ) mode = 1;
  else if ( isUSB == 1 && cwMode == 1 ) mode = 2;
  else if ( isUSB == 0 && cwMode == 1 ) mode = 3;
  if (vfoActive == VFO_A) band = VfoaBand;
  else band = VfobBand;

  modifier = band * 10 + mode;
  New_Memory_Frequency = (Freq * 10) + modifier;


  EEPROM.put(MEMBANK01_0 + (Last_Memory_Selected * 4) + (bank * 40), New_Memory_Frequency);

  Display_Memory_Channel( bank, Last_Memory_Selected );
}



void doMem( void ) {
  int bank = 0;
  int Last_Memory_Selected = 0;
  bool scanMemButtons = true;
  bool Mem_Selected = false;



  Display_Bank(bank);




  while ( scanMemButtons ) {
    while (!readTouch()) active_delay(100);

    scaleTouch(&ts_point);
    for (int i = 0; i < MAX_MEMORIES; i++) {
      struct MemButton b;
      memcpy_P(&b, MemButtons + i, sizeof(struct Button));

      int x2 = b.x + b.w;
      int y2 = b.y + b.h;

      if (b.x < ts_point.x && ts_point.x < x2 &&
          b.y < ts_point.y && ts_point.y < y2) {
        Select_Memory_Channel(Last_Memory_Selected, DISPLAY_DARKGREY);
        Select_Memory_Channel(i, DISPLAY_WHITE);
        Last_Memory_Selected = i;
        Mem_Selected = true;
      }
    }
    if (240 < ts_point.x && ts_point.x < 310 &&
        200 < ts_point.y && ts_point.y < 234) {
      scanMemButtons = false;

    } else if (2 < ts_point.x && ts_point.x < 72 &&
               200 < ts_point.y && ts_point.y < 234) {
      if ( Mem_Selected  ) {
        Process_Load ( bank, Last_Memory_Selected );
        Mem_Selected = false;
      }

    } else if (80 < ts_point.x && ts_point.x < 150 &&
               200 < ts_point.y && ts_point.y < 234) {
      if ( Mem_Selected  ) {
        Process_Store ( bank, Last_Memory_Selected, frequency );
        Mem_Selected = false;
      }

    } else if (166 < ts_point.x && ts_point.x < 236 &&
               200 < ts_point.y && ts_point.y < 234) {
      if ( Mem_Selected  ) {
        Process_Store ( bank, Last_Memory_Selected, 600000000L );
        Mem_Selected = false;
      }
    }

    while (readTouch()) active_delay(100);
  }
  guiUpdate();
  updateDisplay();
}

void Do_Band_Menu(void) {
  int tempBand = 0;

  tft.fillRect(2, 90, 310, 110, DISPLAY_BLUE);
  tft.setTextSize(2);
  displayText((char*)"  80   40   30   20 ", 30, 100, 265, 30, DISPLAY_YELLOW, DISPLAY_BLACK, DISPLAY_YELLOW, true);
  tft.setTextSize(2);
  displayText((char*)"  17   15   10   G/C", 30, 160, 265, 30, DISPLAY_YELLOW, DISPLAY_BLACK, DISPLAY_YELLOW, true);
  while (readTouch() )active_delay(100);
  while (!readTouch() ) active_delay(100);

  scaleTouch(&ts_point);
  // process selection
  if (100 <= ts_point.y && ts_point.y <= 130) {
    if ( 30 <= ts_point.x && ts_point.x <= 95)tempBand = 0;
    else if ( 96 <= ts_point.x && ts_point.x <= 161)tempBand = 1;
    else if ( 162 <= ts_point.x && ts_point.x <= 227)tempBand = 2;
    else if ( 228 <= ts_point.x && ts_point.x <= 295)tempBand = 3;
    switchBand(tempBand, true);
    display_Mode();
  }
  if (160 <= ts_point.y && ts_point.y <= 190) {
    if ( 30 <= ts_point.x && ts_point.x <= 95)tempBand = 4;
    else if ( 96 <= ts_point.x && ts_point.x <= 161)tempBand = 5;
    else if ( 162 <= ts_point.x && ts_point.x <= 227)tempBand = 6;
    else if ( 228 <= ts_point.x && ts_point.x <= 295)tempBand = 7;
    switchBand(tempBand, true);
    display_Mode();
  }

  tft.fillRect(2, 90, 310, 110, DISPLAY_BLUE);
  if (vfoActive == VFO_A)displayVFO(VFO_A);
  else displayVFO(VFO_B);

#ifdef KEYBOARD_INCLUDED
  Clear_Keyboard();
#endif
}




void DoMenuMode(void) {

  tft.setTextSize(2);
  displayText((char*)"  USB  LSB  CW  CWR  CWP", 2, 160, 315, 30, DISPLAY_YELLOW, DISPLAY_BLACK, DISPLAY_YELLOW, true);
  while (readTouch() )active_delay(100);
  while (!readTouch() ) active_delay(100);
  scaleTouch(&ts_point);
  if (150 <= ts_point.y && ts_point.y <= 190) {
    if ( 10 <= ts_point.x && ts_point.x <= 70)Current_Mode = 0;
    else if ( 72 <= ts_point.x && ts_point.x <= 130)Current_Mode = 1;
    else if ( 132 <= ts_point.x && ts_point.x <= 190)Current_Mode = 2;
    else if ( 192 <= ts_point.x && ts_point.x <= 250)Current_Mode = 3;
    else if ( 252 <= ts_point.x && ts_point.x <= 310)Current_Mode = 4;
    Set_Mode();
    display_Mode();
    setFrequency(frequency);
    if ( vfoActive == VFO_A )BandModeVFOa[Current_Band] = Current_Mode;
    else BandModeVFOb[Current_Band] = Current_Mode;
    TimeToSaveFrequency = TimeUntilSave_Frequency;

  }
  displayText((char*)" ", 2, 160, 315, 30, DISPLAY_BLUE, DISPLAY_BLUE, DISPLAY_BLUE, true);
#ifdef KEYBOARD_INCLUDED
  Clear_Keyboard();
#endif

}

void doCommand( void ) {


  // check menu bar change
  if ( (132 <= ts_point.x && ts_point.x <= 182) ) {
    Active_Bar_Menu++;
    if ( Active_Bar_Menu > MAXMENUS)Active_Bar_Menu = 1;
    display_Menu_Bar();
  }

  // menu entry one
  else if ( (2 <= ts_point.x && ts_point.x <= 72) && (Active_Bar_Menu == 1)) Do_Band_Menu();
  else if ( (2 <= ts_point.x && ts_point.x <= 72) && (Active_Bar_Menu == 2)) ritToggle();
  else if ( (2 <= ts_point.x && ts_point.x <= 72) && (Active_Bar_Menu == 3)) {
    TX_Inhibit = true;
    doSetup2();
    TX_Inhibit = false;
  }

  // menu entry two
  else if ( (60 <= ts_point.x && ts_point.x <= 126) && (Active_Bar_Menu == 1)) {
    TX_Inhibit = true;
    enterFreq();
    while ( readTouch() )active_delay(100);
    TX_Inhibit = false;
  }
  else if ( (60 <= ts_point.x && ts_point.x <= 126) && (Active_Bar_Menu == 2)) splitToggle();
  else if ( (60 <= ts_point.x && ts_point.x <= 126) && (Active_Bar_Menu == 3)) {
    EEPROM.get(CW_SPEED, cwSpeed);
    sprintf(c, "%02d", wpm);
    if (Spd_Active) {
      Spd_Active = false;
      tft.setTextSize(3);
      displayText(c, 240, 38, 44, 33, DISPLAY_GREEN, DISPLAY_BLUE, DISPLAY_BLUE, false);
      tft.setTextSize(2);
      tft.setCursor(280, 52);
      tft.print("wpm");
    }
    else {
      Spd_Active = true;
      tft.setTextSize(3);
      displayText(c, 240, 38, 44, 33, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, false);
      tft.setTextSize(2);
      tft.setCursor(280, 52);
      tft.print("wpm");
    }
  }

  // menu entry three
  else if ( (186 <= ts_point.x && ts_point.x <= 254) && (Active_Bar_Menu == 1)) DoMenuMode();
  else if ( (186 <= ts_point.x && ts_point.x <= 254) && (Active_Bar_Menu == 2)) CopyAtoB();


  // menu entry four
  else if ( (256 <= ts_point.x && ts_point.x <= 318) && (Active_Bar_Menu == 1)) {
    TX_Inhibit = true;
    doMem();
    TX_Inhibit = false;
  }
  else if ( (256 <= ts_point.x && ts_point.x <= 318) && (Active_Bar_Menu == 2)) {
    LckToggle();
    displayVFO(VFO_A);
    displayVFO(VFO_B);
  }

  LastSmeter = 0;
}

// From PJRC forum member Silverlock
// special code for T4.0 to set RTC latch

void local_rtc_set(unsigned long t)
{
  // stop the RTC
  SNVS_HPCR &= ~(SNVS_HPCR_RTC_EN | SNVS_HPCR_HP_TS);
  while (SNVS_HPCR & SNVS_HPCR_RTC_EN); // wait
  // stop the SRTC
  SNVS_LPCR &= ~SNVS_LPCR_SRTC_ENV;
  while (SNVS_LPCR & SNVS_LPCR_SRTC_ENV); // wait
  // set the SRTC
  SNVS_LPSRTCLR = t << 15;
  SNVS_LPSRTCMR = t >> 17;
  // start the SRTC
  SNVS_LPCR |= SNVS_LPCR_SRTC_ENV;
  while (!(SNVS_LPCR & SNVS_LPCR_SRTC_ENV)); // wait
  // start the RTC and sync it to the SRTC
  SNVS_HPCR |= SNVS_HPCR_RTC_EN | SNVS_HPCR_HP_TS;
}


void display_Menu_Bar( void) {

  tft.setTextSize(2);
  if ( Active_Bar_Menu == 0 ) {
    tft.fillRect(2, 200, 315, 40, DISPLAY_BLUE);
  } else if ( Active_Bar_Menu == 1 ) {
    displayText((char*)" Band Freq      Mode Chan", 1, 200, 318, 30, DISPLAY_WHITE, DISPLAY_NAVY, DISPLAY_WHITE, true);
    //displayText((char*)"  Band  Freq  Mode  Chan", 2, 200, 315, 30, DISPLAY_WHITE, DISPLAY_NAVY, DISPLAY_WHITE, true);
  } else if ( Active_Bar_Menu == 2 ) {
    displayText((char*)" RIT Split      A->B Lock", 1, 200, 318, 30, DISPLAY_WHITE, DISPLAY_NAVY, DISPLAY_WHITE, true);
    tft.setTextSize(2);
    tft.setTextColor(DISPLAY_GREEN);
    if ( ritOn ) {
      tft.setCursor(5, 208);
      tft.print((char*)" RIT ");
    }
    if ( splitOn ) {
      tft.setCursor(65, 208);
      tft.print((char*)"Split");
    }
    tft.setTextSize(2);
    tft.setCursor(257, 208);
    if ( Lck_Active ) {
      tft.setTextColor(DISPLAY_GREEN);
      tft.print((char*)"Lock");
    }
  } else if ( Active_Bar_Menu == 3 ) {
    displayText((char*)" Set ", 1, 200, 318, 30, DISPLAY_WHITE, DISPLAY_NAVY, DISPLAY_WHITE, true);
  }
  tft.fillTriangle(146, 222, 158, 206, 168, 222, DISPLAY_WHITE);
  tft.drawLine(132, 200, 132, 228,  DISPLAY_WHITE);
  tft.drawLine(182, 200, 182, 228,  DISPLAY_WHITE);

}


//returns true if the button is pressed
int btnDown() {
  if (digitalRead(FBUTTON) == HIGH)
    return 0;
  else
    return 1;
}


void checkButton( void ) {
  int i = 0;

  if (btnDown()) {
    while (btnDown() && (i < 50)) {
      delay(10);
      i++;
    }
    if ( i > 30 ) {
      Active_Bar_Menu++;
      if ( Active_Bar_Menu > MAXMENUS)Active_Bar_Menu = 1;
      display_Menu_Bar();
    }
    while (btnDown()) delay(10);

  }

}



void  checkTouch() {

  bool menu_on = true;
  int Hrh, Hrl, Minh, Minl, Sech, Secl;
  if (!readTouch() )return;
  scaleTouch(&ts_point);

  // process TOD 0,1,100,20
  if ( RTC_Clock &&
       ( 1 <= ts_point.y && ts_point.y < 40 ) &&
       ( 1 <= ts_point.x && ts_point.x <= 100 )) {
    TX_Inhibit = true;
    Hrh = hour() / 10;
    Hrl = hour() - Hrh * 10;
    Minh = minute() / 10;
    Minl = minute() - Minh * 10;
    Sech = second() / 10;
    Secl = second() - Sech * 10;


    tft.fillScreen(DISPLAY_BLUE);
    tft.setTextSize(2);
    sprintf(c, "Press digits to set time");
    displayText(c, 2, 6, 100, 20, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, false);
    tft.setTextSize(2);
    sprintf(c, "Enter: to start new time");
    displayText(c, 2, 32, 100, 20, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, false);
    tft.setTextSize(2);
    sprintf(c, "Quit: for no change");
    displayText(c, 1, 58, 100, 20, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, false);
    tft.setTextSize(2);
    displayText((char*)" Hours   Minutes  Seconds", 0, Time_Entrie[1].y - 30, Time_Entrie[1].w, Time_Entrie[1].h, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, false);
    displayText((char*)"Enter", Time_Entrie[6].x - 60, Time_Entrie[6].y, Time_Entrie[6].w, Time_Entrie[6].h, DISPLAY_WHITE, DISPLAY_DARKGREEN, DISPLAY_WHITE, false);
    displayText((char*)" Quit",  Time_Entrie[6].x + 60, Time_Entrie[6].y, Time_Entrie[6].w, Time_Entrie[6].h, DISPLAY_WHITE, DISPLAY_RED, DISPLAY_WHITE, false);

    sprintf(c, "% 01d", Hrh);
    displayText(c, Time_Entrie[0].x, Time_Entrie[0].y, Time_Entrie[0].w, Time_Entrie[0].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_WHITE, true);
    sprintf(c, "% 01d", Hrl);
    displayText(c, Time_Entrie[1].x, Time_Entrie[1].y, Time_Entrie[1].w, Time_Entrie[1].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_WHITE, true);

    sprintf(c, "% 01d", Minh);
    displayText(c, Time_Entrie[2].x, Time_Entrie[2].y, Time_Entrie[2].w, Time_Entrie[2].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_WHITE, false);
    sprintf(c, "% 01d", Minl);
    displayText(c, Time_Entrie[3].x, Time_Entrie[3].y, Time_Entrie[3].w, Time_Entrie[3].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_WHITE, false);

    sprintf(c, "% 01d", Sech);
    displayText(c, Time_Entrie[4].x, Time_Entrie[4].y, Time_Entrie[4].w, Time_Entrie[4].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_WHITE, false);
    sprintf(c, "% 01d", Secl);
    displayText(c, Time_Entrie[5].x, Time_Entrie[5].y, Time_Entrie[5].w, Time_Entrie[5].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_WHITE, false);

    while ( menu_on ) {
      while (!readTouch() ) active_delay(100);
      scaleTouch(&ts_point);
      // Check to buttons time
      if ( ( Time_Entrie[6].y <= ts_point.y && ts_point.y < Time_Entrie[6].y + Time_Entrie[6].h  )) {
        if ( (Time_Entrie[6].x - 60 <= ts_point.x) && (ts_point.x < (Time_Entrie[6].x - 60 + Time_Entrie[6].w)) ) {
          menu_on = false;
          setTime((Hrh * 10) + Hrl, (Minh * 10) + Minl, (Sech * 10) + Secl, day(), month(), year());
          rtc_set(now()); // set the RTC
        } else if ( (Time_Entrie[6].x + 60 <= ts_point.x) && (ts_point.x < (Time_Entrie[6].x + 60 + Time_Entrie[6].w)) )
          menu_on = false;
      } else {
        // Check if touch inrange of digits
        if ( Time_Entrie[0].y <= ts_point.y && ts_point.y < Time_Entrie[0].y + Time_Entrie[0].h  ) {
          // check which digit
          if ( Time_Entrie[0].x <= ts_point.x && ts_point.x < Time_Entrie[0].x + Time_Entrie[0].w ) {
            if ( Hrh++ >= 2 )Hrh = 0;
            sprintf(c, "% 01d", Hrh );
            displayText(c, Time_Entrie[0].x, Time_Entrie[0].y, Time_Entrie[0].w, Time_Entrie[0].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_DARKGREY, false);
          } else if ( Time_Entrie[1].x <= ts_point.x && ts_point.x < Time_Entrie[1].x + Time_Entrie[1].w ) {
            if ( Hrl++ >= 9 )Hrl = 0;
            sprintf(c, "% 01d", Hrl);
            displayText(c, Time_Entrie[1].x, Time_Entrie[1].y, Time_Entrie[1].w, Time_Entrie[1].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_DARKGREY, false);
          } else if ( Time_Entrie[2].x <= ts_point.x && ts_point.x < Time_Entrie[2].x + Time_Entrie[2].w ) {
            if ( Minh++ >= 5 )Minh = 0;
            sprintf(c, "% 01d", Minh);
            displayText(c, Time_Entrie[2].x, Time_Entrie[2].y, Time_Entrie[2].w, Time_Entrie[2].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_DARKGREY, false);
          } else if ( Time_Entrie[3].x <= ts_point.x && ts_point.x < Time_Entrie[3].x + Time_Entrie[3].w ) {
            if ( Minl++ >= 9 )Minl = 0;
            sprintf(c, "% 01d", Minl);
            displayText(c, Time_Entrie[3].x, Time_Entrie[3].y, Time_Entrie[3].w, Time_Entrie[3].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_DARKGREY, false);
          } else if ( Time_Entrie[4].x <= ts_point.x && ts_point.x < Time_Entrie[4].x + Time_Entrie[4].w ) {
            if ( Sech++ >= 5 )Sech = 0;
            sprintf(c, "% 01d", Sech);
            displayText(c, Time_Entrie[4].x, Time_Entrie[4].y, Time_Entrie[4].w, Time_Entrie[4].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_DARKGREY, false);
          } else if ( Time_Entrie[5].x <= ts_point.x && ts_point.x < Time_Entrie[5].x + Time_Entrie[5].w ) {
            if ( Secl++ >= 9 )Secl = 0;
            sprintf(c, "% 01d", Secl);
            displayText(c, Time_Entrie[5].x, Time_Entrie[5].y, Time_Entrie[5].w, Time_Entrie[5].h, DISPLAY_GREEN, DISPLAY_BLACK, DISPLAY_DARKGREY, false);
          }
        }
      }
      while (readTouch() )active_delay(100);
    }
    guiUpdate();
    Display_Freq_Underline();
    TX_Inhibit = false;
    LastSmeter = 0;

    delay(1000);
#ifdef KEYBOARD_INCLUDED
  Clear_Keyboard();
#endif

    //58, 94, 130, 166, 238, 274
    // process freq incr presses
  } else if ( 90 <= ts_point.y && ts_point.y < 156 ) {
    if (58 <= ts_point.x && ts_point.x <= 90 ) {
      Freq_Index = 0;
      Display_Freq_Underline();
    }
    if (94 <= ts_point.x && ts_point.x <= 124 ) {
      Freq_Index = 1;
      Display_Freq_Underline();
    }
    if (130 <= ts_point.x && ts_point.x <= 160 ) {
      Freq_Index = 2;
      Display_Freq_Underline();
    }
    if (166 <= ts_point.x && ts_point.x <= 232 ) {
      Freq_Index = 3;
      Display_Freq_Underline();
    }
    if (238 <= ts_point.x && ts_point.x < 271 ) {
      Freq_Index = 4;
      Display_Freq_Underline();
    }
    if (274 <= ts_point.x && ts_point.x <= 305 ) {
      Freq_Index = 5;
      Display_Freq_Underline();
    }
  }
  // swap vfo
  if ( ( 48 <= ts_point.y && ts_point.y < 85 ) &&
       ( 0 <= ts_point.x && ts_point.x <= 110 )) {
    if ( !ritOn ) {
      if (vfoActive == VFO_B)
        switchVFO(VFO_A, true);
      else
        switchVFO(VFO_B, true);
    }
  }

  // process cwspeed
  //230, 42, 44, 33
  if ( ( 40 <= ts_point.y && ts_point.y < 80 ) &&
       ( 250 <= ts_point.x && ts_point.x <= 310 )) {
    EEPROM.get(CW_SPEED, cwSpeed);
    sprintf(c, "%02d", wpm);
    if (Spd_Active) {
      Spd_Active = false;
      tft.setTextSize(3);
      displayText(c, 240, 38, 44, 33, DISPLAY_GREEN, DISPLAY_BLUE, DISPLAY_BLUE, false);
      tft.setTextSize(2);
      tft.setCursor(280, 52);
      tft.print("wpm");
    }
    else {
      Spd_Active = true;
      tft.setTextSize(3);
      displayText(c, 240, 38, 44, 33, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, false);
      tft.setTextSize(2);
      tft.setCursor(280, 52);
      tft.print("wpm");
    }

  }

  // DoMenuMode();
  // 180, 38, 44, 33
  if ( ( 30 <= ts_point.y && ts_point.y < 85 ) &&
       ( 180 <= ts_point.x && ts_point.x <= 225 )) DoMenuMode();


  // check band
  //116, 38, 44, 33
  if ( ( 30 <= ts_point.y && ts_point.y < 85 ) &&
       ( 115 <= ts_point.x && ts_point.x <= 170 )) Do_Band_Menu();

  //    scaleTouch(&ts_point);
  if ( Active_Bar_Menu > 0 ) {

    if (200 <= ts_point.y && ts_point.y <= 230) {
      doCommand();
    }
    while (readTouch()) active_delay(100);
  }
}

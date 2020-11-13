#include <Arduino.h>
#include <EEPROM.h>
#include "ubitx.h"
#include "nano_gui.h"
#include <Adafruit_ILI9341.h>

#ifdef KEYBOARD_INCLUDED
#include <PS2Keyboard.h>
extern PS2Keyboard keyboard;
#endif

extern Adafruit_ILI9341 tft;
extern struct Point ts_point;
extern void setCwTone();
extern void drawSmeter(void);
extern volatile int smeterInterval;

extern bool Smtr_Active;

/** Menus
    The Radio menus are accessed by tapping on the function button.
    - The main loop() constantly looks for a button press and calls doMenu() when it detects
    a function button press.
    - As the encoder is rotated, at every 10th pulse, the next or the previous menu
    item is displayed. Each menu item is controlled by it's own function.
    - Eache menu function may be called to display itself
    - Each of these menu routines is called with a button parameter.
    - The btn flag denotes if the menu itme was clicked on or not.
    - If the menu item is clicked on, then it is selected,
    - If the menu item is NOT clicked on, then the menu's prompt is to be displayed
*/
int Smtr_Constant;
extern int Region;
extern bool RTC_Clock;

void setupExit() {
  menuOn = 0;
}

struct Menu_Pos {
  int x, y, w, h;
};
const struct Menu_Pos Menu_Position[9] PROGMEM = {
  { 30, 20, 240, 28 },  // smeter
  { 30, 50, 240, 28 },  // tone
  { 30, 80, 240, 28 },  // CW dly
  { 30, 110, 240, 28 }, // Paddle
  { 30, 140, 80, 28 }, // Clock
  { 120, 140, 80, 28 }, // Bandplan
  { 30, 170, 130, 28 }, // CAL FREQ
  { 160, 170, 100, 28 }, // CAL BFO
  { 200, 200, 80, 28 }, // EXIT
};

//this is used by the si5351 routines in the ubitx_5351 file
extern int32_t calibration;
extern uint32_t si5351bx_vcoa;

int Last_Selected_Button = 0;
bool Execute_Button;
bool continue_operation;
bool Cal_Freq_In_Progress = false;


void Process_Input_Freq_Cal ( void) {
  int knob = 0;
  bool continue_loop = true;

  ltoa(calibration, b, 10);
  tft.setTextSize(3);
  displayText(b, 150, 87, 118, 38, DISPLAY_CYAN, DISPLAY_BLACK, DISPLAY_WHITE, false);
  while ( continue_loop ) {
    if (readTouch()) {
      while (readTouch()) active_delay(100);
      scaleTouch(&ts_point);
      if ( (ts_point.x >= 24) && (ts_point.x <= 90) && (ts_point.y >= 126) && (ts_point.y <= 164)) {
        calibration = 875;
        si5351bx_setfreq(0, usbCarrier);  //set back the cardrier oscillator anyway, cw tx switches it off
        si5351_set_calibration(calibration);

        ltoa(calibration, b, 10);
        tft.setTextSize(3);
        displayText(b, 150, 87, 118, 38, DISPLAY_CYAN, DISPLAY_BLACK, DISPLAY_WHITE, false);
      } else if ( (ts_point.x >= 24) && (ts_point.x <= 90) && (ts_point.y >= 170) && (ts_point.y <= 208)) {
        continue_operation = false;
        continue_loop = false;
        Execute_Button = false;
      } else if ( (ts_point.x >= 100) && (ts_point.x <= 166) && (ts_point.y >= 170) && (ts_point.y <= 208)) {
        EEPROM.put(MASTER_CAL, calibration);
        initOscillators();
        si5351_set_calibration(calibration);
        setFrequency(frequency);
        continue_operation = false;
        continue_loop = false;
        Execute_Button = false;
      } else if ( (ts_point.x >= 176) && (ts_point.x <= 280) && (ts_point.y >= 170) && (ts_point.y <= 208)) {
        EEPROM.get(MASTER_CAL, calibration);
        initOscillators();
        si5351_set_calibration(calibration);
        setFrequency(frequency);
        ltoa(calibration, b, 10);
        tft.setTextSize(3);
        displayText(b, 150, 87, 118, 38, DISPLAY_CYAN, DISPLAY_BLACK, DISPLAY_WHITE, false);
        continue_loop = true;
      } else continue_loop = true;
    }
    else if ( btnDown() ) {
      tft.drawRect(150, 87, 130, 38, DISPLAY_DARKGREY);
      Last_Selected_Button = 3;
      while ( btnDown() )active_delay(10);
      continue_loop = false;
    } else {
      knob = enc_read();
      if (knob != 0) {
        calibration += knob * 875;

        si5351bx_setfreq(0, usbCarrier);  //set back the cardrier oscillator anyway, cw tx switches it off
        si5351_set_calibration(calibration);
        setFrequency(frequency);
        ltoa(calibration, b, 10);
        displayText(b, 150, 87, 118, 38, DISPLAY_CYAN, DISPLAY_BLACK, DISPLAY_WHITE, false);
      }
    }
  }

}


void  Display_Cal_Freq_Buttons( void ) {

  // clear freq button
  tft.drawRoundRect(150, 87, 118, 38, 10, DISPLAY_DARKGREY);
  tft.drawRoundRect(24, 126, 66, 38, 10, DISPLAY_DARKGREY);
  tft.drawRoundRect(24, 170, 66, 38, 10, DISPLAY_DARKGREY);
  tft.drawRoundRect(100, 170, 66, 38, 10, DISPLAY_DARKGREY);
  tft.drawRoundRect(176, 170, 104, 38,  10, DISPLAY_DARKGREY);
  switch (Last_Selected_Button) {
    case  0:
      tft.drawRoundRect(150, 87, 118, 38, 10, DISPLAY_WHITE);
      break;
    case  1:
      tft.drawRoundRect(24, 126, 66, 38, 10, DISPLAY_WHITE);
      break;
    case  2:
      tft.drawRoundRect(24, 170, 66, 38, 10, DISPLAY_WHITE);
      break;
    case  3:
      tft.drawRoundRect(100, 170, 66, 38, 10, DISPLAY_WHITE);
      break;
    case  4:
      tft.drawRoundRect(176, 170, 104, 38,  10, DISPLAY_WHITE);
      break;
  }
}


// move selection around and return what is pressed
void Process_Freq_Cal_Buttons ( void ) {
  int knob;

  Display_Cal_Freq_Buttons();
  while ( continue_operation ) {
    while ( !Execute_Button ) {
      knob = enc_read();
      if ( knob != 0 ) {
        if ( knob > 5 ) {
          Last_Selected_Button++;
          if ( Last_Selected_Button > 4 ) Last_Selected_Button = 0;
        } else if ( knob < -5 ) {
          Last_Selected_Button--;
          if ( Last_Selected_Button < 0 ) Last_Selected_Button = 4;
        }
        Display_Cal_Freq_Buttons();
        active_delay(400);
      }
      if ( btnDown() || readTouch() ) {
        Execute_Button = true;
        while ( btnDown() ) active_delay(40);
      }
    }
    switch ( Last_Selected_Button ) {
      case 0:
        Process_Input_Freq_Cal();
        Execute_Button = false;
        break;
      case 1:
        calibration = 875;
        //frequency = 10000000L;
        si5351bx_setfreq(0, usbCarrier);  //set back the cardrier oscillator anyway, cw tx switches it off
        si5351_set_calibration(calibration);
        // setFrequency(frequency);

        ltoa(calibration, b, 10);
        tft.setTextSize(3);
        displayText(b, 150, 87, 130, 34, DISPLAY_CYAN, DISPLAY_NAVY, DISPLAY_WHITE, false);
        Last_Selected_Button = 0;  // force selection to be freq
        Execute_Button = true;            // force immediate execution
        continue_operation = true;
        break;
      case 2:
        continue_operation = false;
        break;
      case 3:
        EEPROM.put(MASTER_CAL, calibration);
        initOscillators();
        si5351_set_calibration(calibration);
        setFrequency(frequency);

        continue_operation = false;
        break;
      case 4:
        EEPROM.get(MASTER_CAL, calibration);
        si5351bx_setfreq(0, usbCarrier);  //set back the cardrier oscillator anyway, cw tx switches it off
        si5351_set_calibration(calibration);

        ltoa(calibration, b, 10);
        tft.setTextSize(3);
        displayText(b, 150, 87, 130, 34, DISPLAY_CYAN, DISPLAY_NAVY, DISPLAY_WHITE, false);
        Last_Selected_Button = 0;  // force selection to be freq
        Execute_Button = true;            // force immediate execution
        continue_operation = true;
        break;
    }
    Display_Cal_Freq_Buttons();
  }
}


void setupFreq() {

  // set up window
  tft.fillScreen(DISPLAY_BLACK);
  Cal_Freq_In_Progress = true;
  tft.drawRect(10, 10, 300, 220, DISPLAY_WHITE);
  tft.drawRect(12, 12, 296, 216, DISPLAY_WHITE);
  displayRawText((char*)"Calibrate Frequency", 40, 20, DISPLAY_WHITE, DISPLAY_BLACK, 2);

  tft.setTextSize(1);
  displayText((char*)" FACTORY", 24, 126, 66, 38, DISPLAY_YELLOW, DISPLAY_RED, DISPLAY_DARKGREY, true);
  tft.setTextSize(2);
  tft.setCursor(28, 146);
  tft.print("RESET");

  tft.setTextSize(1);
  displayText((char*)"   USER", 24, 170, 66, 38, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_DARKGREY, true);
  tft.setTextSize(2);
  tft.setCursor(34, 190);
  tft.print("EXIT");

  tft.setTextSize(1);
  displayText((char*)"   USER", 100, 170, 66, 38, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_DARKGREY, true);
  tft.setTextSize(2);
  tft.setCursor(110, 190);
  tft.print("SAVE");
  tft.setTextSize(1);
  displayText((char*)"      USER", 176, 170, 104, 38, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_DARKGREY, true);
  tft.setTextSize(2);
  tft.setCursor(186, 190);
  tft.print("RESTORE");
  //round off the the nearest khz
  EEPROM.get(MASTER_CAL, calibration);
  frequency = (frequency / 1000l) * 1000l;
  setFrequency(frequency);
  displayRawText((char*)"You should have a", 20, 50, DISPLAY_CYAN, DISPLAY_BLACK, 1);
  displayRawText((char*)"signal exactly at ", 20, 66, DISPLAY_CYAN, DISPLAY_BLACK, 1);
  ltoa(frequency / 1000l, c, 10);
  strcat(c, " K");
  displayRawText(c, 150, 50, DISPLAY_CYAN, DISPLAY_BLACK, 3);
  displayRawText((char*)"Rotate to", 20, 80, DISPLAY_YELLOW, DISPLAY_BLACK, 2);
  displayRawText((char*)"zerobeat", 20, 96, DISPLAY_YELLOW, DISPLAY_BLACK, 2);

  while (!btnDown() && (readTouch())) active_delay(100);
  active_delay(100);



  Last_Selected_Button = 0;  // force selection to be freq
  Execute_Button = true;            // force immediate execution
  continue_operation = true;
  Process_Freq_Cal_Buttons();
  Cal_Freq_In_Progress = false;

  //debounce and delay
  while (btnDown())active_delay(50);
  active_delay(100);
  while (readTouch())active_delay(100);
}
//////////////////////////////////////////////////////////////////////////////////


void  Display_Cal_BFO_Buttons( void ) {

  // clear freq button
  tft.drawRoundRect(100, 126, 180, 38, 10, DISPLAY_CYAN);
  tft.drawRoundRect(24, 126, 66, 38, 10, DISPLAY_DARKGREY);
  tft.drawRoundRect(24, 170, 66, 38, 10, DISPLAY_DARKGREY);
  tft.drawRoundRect(100, 170, 66, 38, 10, DISPLAY_DARKGREY);
  tft.drawRoundRect(176, 170, 104, 38,  10, DISPLAY_DARKGREY);
  switch (Last_Selected_Button) {
    case  0:
      tft.drawRoundRect( 100, 126, 180, 38, 10, DISPLAY_WHITE);
      break;
    case  1:
      tft.drawRoundRect(24, 126, 66, 38, 10, DISPLAY_WHITE);
      break;
    case  2:
      tft.drawRoundRect(24, 170, 66, 38, 10, DISPLAY_WHITE);
      break;
    case  3:
      tft.drawRoundRect(100, 170, 66, 38, 10, DISPLAY_WHITE);
      break;
    case  4:
      tft.drawRoundRect(176, 170, 104, 38,  10, DISPLAY_WHITE);
      break;
  }
}

// move selection around and return what is pressed
void Process_BFO_Cal_Buttons ( void ) {
  int knob;
  bool continue_loop = true;

  Display_Cal_BFO_Buttons();

  while ( continue_operation ) {
    while ( !Execute_Button ) {
      knob = enc_read();
      if ( knob != 0 ) {
        if ( knob > 5 ) {
          Last_Selected_Button++;
          if ( Last_Selected_Button > 4 ) Last_Selected_Button = 0;
        } else if ( knob < -5 ) {
          Last_Selected_Button--;
          if ( Last_Selected_Button < 0 ) Last_Selected_Button = 4;
        }
        Display_Cal_BFO_Buttons();
        active_delay(200);
      }
      if ( btnDown() || readTouch() ) {
        Execute_Button = true;
        while ( btnDown() || readTouch()) active_delay(10);
      }
    }
    switch ( Last_Selected_Button ) {
      case 0:
        while ( continue_loop ) {
          if (readTouch()) {
            while (readTouch()) active_delay(100);
            scaleTouch(&ts_point);
            if ( (ts_point.x >= 24) && (ts_point.x <= 90) && (ts_point.y >= 126) && (ts_point.y <= 164)) {
              usbCarrier = 11055000L;
              // setFrequency(7100000L);
              si5351bx_setfreq(0, usbCarrier);
              printCarrierFreq(usbCarrier);
              Last_Selected_Button = 0;
              continue_loop = true;
              Execute_Button = true;
            } else if ( (ts_point.x >= 24) && (ts_point.x <= 90) && (ts_point.y >= 170) && (ts_point.y <= 208)) {
              continue_operation = false;
              continue_loop = false;
              Execute_Button = false;
            } else if ( (ts_point.x >= 100) && (ts_point.x <= 166) && (ts_point.y >= 170) && (ts_point.y <= 208)) {
              EEPROM.put(USB_CAL, usbCarrier);
              continue_operation = false;
              continue_loop = false;
              Execute_Button = false;
            } else if ( (ts_point.x >= 176) && (ts_point.x <= 280) && (ts_point.y >= 170) && (ts_point.y <= 208)) {
              EEPROM.get(USB_CAL, usbCarrier);
              si5351bx_setfreq(0, usbCarrier);
              tft.setTextSize(3);
              printCarrierFreq(usbCarrier);
              continue_loop = true;
            } else continue_loop = true;
          } else {
            knob = enc_read();

            if (knob != 0) {
              usbCarrier -= 50 * knob;

              si5351bx_setfreq(0, usbCarrier);
              setFrequency(frequency);
              printCarrierFreq(usbCarrier);
            }
            active_delay(100);
          }
          if ( btnDown() ) {
            while ( btnDown()) active_delay(10);
            continue_loop = false;
            Execute_Button = false;
          }

        }
        if ( (Last_Selected_Button == 1) || (Last_Selected_Button == 4)) {
          Last_Selected_Button = 3;
          continue_operation = true;
          continue_loop = true;
          Execute_Button = false;
        }
        break;
      case 1:
        usbCarrier = 11055000L;
        setFrequency(7100000L);
        si5351bx_setfreq(0, usbCarrier);
        printCarrierFreq(usbCarrier);
        Last_Selected_Button = 0;
        continue_loop = true;
        Execute_Button = true;
        break;
      case 2:
        continue_operation = false;
        break;
      case 3:
        EEPROM.put(USB_CAL, usbCarrier);
        continue_operation = false;
        break;
      case 4:
        EEPROM.get(USB_CAL, usbCarrier);
        si5351bx_setfreq(0, usbCarrier);
        tft.setTextSize(3);
        printCarrierFreq(usbCarrier);
        Last_Selected_Button = 0;  // force selection to be freq
        Execute_Button = true;            // force immediate execution
        continue_operation = true;
        continue_loop = true;
        break;
    }
    Display_Cal_BFO_Buttons();
  }
}


void setupBFO() {


  tft.fillScreen(DISPLAY_BLACK);
  displayRawText((char*)"Calibrate BFO", 70, 20, DISPLAY_WHITE, DISPLAY_BLACK, 2);

  tft.setTextSize(1);
  displayText((char*)" FACTORY", 24, 126, 66, 38, DISPLAY_YELLOW, DISPLAY_RED, DISPLAY_DARKGREY, true);
  tft.setTextSize(2);
  tft.setCursor(28, 146);
  tft.print("RESET");

  tft.setTextSize(1);
  displayText((char*)"   USER", 24, 170, 66, 38, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_DARKGREY, true);
  tft.setTextSize(2);
  tft.setCursor(34, 190);
  tft.print("EXIT");

  tft.setTextSize(1);
  displayText((char*)"   USER", 100, 170, 66, 38, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_DARKGREY, true);
  tft.setTextSize(2);
  tft.setCursor(110, 190);
  tft.print("SAVE");
  tft.setTextSize(1);
  displayText((char*)"      USER", 176, 170, 104, 38, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_DARKGREY, true);
  tft.setTextSize(2);
  tft.setCursor(186, 190);
  tft.print("RESTORE");
  EEPROM.get(USB_CAL, usbCarrier);
  si5351bx_setfreq(0, usbCarrier);
  tft.setTextSize(3);
  printCarrierFreq(usbCarrier);


  Last_Selected_Button = 0;  // force selection to be freq
  Execute_Button = true;            // force immediate execution
  continue_operation = true;
  Process_BFO_Cal_Buttons();

}





void setupSmtrConstant( void ) {
  int knob;

  sprintf(c, "%02d", Smtr_Constant);
  tft.setTextSize(2);
  displayText(c, 184, 21, 30, 25, DISPLAY_WHITE, DISPLAY_BLACK, DISPLAY_BLACK, false);

  while (!readTouch() ) {
    knob = enc_read();
    if (knob < 0 && Smtr_Constant > 1 )
      Smtr_Constant -= 1;
    else if (knob > 0 && Smtr_Constant < 100)
      Smtr_Constant += 1;
    else
      continue; //don't update the frequency or the display
    sprintf(c, "%02d", Smtr_Constant);
    tft.setTextSize(2);
    displayText(c, 184, 21, 30, 25, DISPLAY_WHITE, DISPLAY_BLACK, DISPLAY_BLACK, true);
  }
  EEPROM.put(SMTRCONSTANT, Smtr_Constant);
  sprintf(c, "%02d", Smtr_Constant);
  tft.setTextSize(2);
  displayText(c, 184, 21, 30, 25, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
  while (readTouch())active_delay(100);

}



void setupCwDelay() {
  int knob = 0;

  sprintf(c, "%04d", (int)cwDelayTime);
  tft.setTextSize(2);
  displayText(c, 160, 82, 60, 24, DISPLAY_WHITE, DISPLAY_BLACK, DISPLAY_BLUE, true);

  while (!btnDown() && (!readTouch())) {
    knob = enc_read();
    if (knob < 0 && cwDelayTime > 10 )
      cwDelayTime -= 10;
    else if (knob > 0 && cwDelayTime < 1000)
      cwDelayTime += 10;
    else
      continue; //don't update the frequency or the display
    sprintf(c, "%04d", (int)cwDelayTime);
    tft.setTextSize(2);
    displayText(c, 160, 82, 60, 24, DISPLAY_WHITE, DISPLAY_BLACK, DISPLAY_BLUE, true);
  }
  EEPROM.put(CW_DELAYTIME, cwDelayTime);
  sprintf(c, "%04d", (int)cwDelayTime);
  tft.setTextSize(2);
  displayText(c, 160, 82, 60, 24, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);

  while (btnDown())
    active_delay(50);
  active_delay(100);
  while (readTouch())active_delay(100);
}



void keyDisplay(void) {

  tft.setTextSize(2);

  switch ( CW_Port_Type ) {
    case PADDLE_NORM:
      displayText((char*) "Normal     Paddle", 30, 110, 240, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
      break;
    case PADDLE_REVERSE:
      displayText((char*) "Reverse    Paddle", 30, 110, 240, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
      break;
    case MANUAL:
      displayText((char*) "Straight   Key    ", 30, 110, 240, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
      break;
    case COOTIE:
      displayText((char*) "Cootie     Key     ", 30, 110, 240, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
      break;
    case KEYBOARD:
      displayText((char*) "PS2 Keyboard       ", 30, 110, 240, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
      break;
  }
}


// code provided by W3IVD

void setupKeyer() {
  /* Rotate through keyer modes */


  switch ( CW_Port_Type) {
    case KEYBOARD:
      CW_Port_Type = PADDLE_NORM;
      break;
    case PADDLE_NORM:
      CW_Port_Type = PADDLE_REVERSE;
      break;
    case PADDLE_REVERSE:
      CW_Port_Type = MANUAL;
      break;
    case MANUAL:
      CW_Port_Type = COOTIE;
      break;
    case COOTIE:
#ifdef KEYBOARD_INCLUDED
      CW_Port_Type = KEYBOARD;
#else
      CW_Port_Type = PADDLE_NORM;
#endif
      break;
  }
  unsigned long temp = CW_Port_Type;
  EEPROM.put(CW_KEY_TYPE, temp);
#ifdef KEYBOARD_INCLUDED
  if ( CW_Port_Type == KEYBOARD ) keyboard.begin(DataPin, IRQpin);
  else {
    pinMode(DIGITAL_DOT, INPUT_PULLUP);
    pinMode(DIGITAL_DASH, INPUT_PULLUP);
  }
#else
    pinMode(DIGITAL_DOT, INPUT_PULLUP);
    pinMode(DIGITAL_DASH, INPUT_PULLUP);
#endif
  keyDisplay();

}

void drawSetupMenu() {


  tft.fillScreen(DISPLAY_BLACK);

  if ( Smtr_Active)sprintf(c, "S meter ON   %02d", Smtr_Constant);
  else sprintf(c,             "S meter OFF      ");
  tft.setTextSize(2);
  displayText(c, 30, 20, 240, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);

  sprintf(c, "Tone            hz ");
  tft.setTextSize(2);
  displayText(c, 30, 50, 240, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
  sprintf(c, "%04d", (int)sideTone);
  tft.setTextSize(2);
  displayText(c, 160, 52, 58, 24, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);

  sprintf(c, "CW Dly          ms ");
  tft.setTextSize(2);
  displayText(c, 30, 80, 240, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
  sprintf(c, "%04d", (int)cwDelayTime);
  tft.setTextSize(2);
  displayText(c, 160, 82, 60, 24, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);

  tft.setTextSize(2);
  keyDisplay();

  tft.setTextSize(2);
  displayText((char*)"CAL FRQ ", 30, 170, 120, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
  tft.setTextSize(2);
  displayText((char*)"CAL BFO ", 154, 170, 116, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
  tft.setTextSize(2);
  if ( RTC_Clock )
    displayText((char*)"Clock On ", 30, 140, 120, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
  else
    displayText((char*)"Clock Off", 30, 140, 120, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
  tft.setTextSize(2);
  displayText((char*)"Exit", 210, 200, 60, 28, DISPLAY_WHITE, DISPLAY_RED, DISPLAY_BLUE, true);
  tft.setTextSize(2);
  if ( Region == 1 )displayText((char*)"Region 1", 154, 140, 116, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
  else if ( Region == 2 )displayText((char*)"Region 2", 154, 140, 116, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
  else if ( Region == 3 )displayText((char*)"Region 3", 154, 140, 116, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);

}

void doSetup2() {

  drawSetupMenu();

  //wait for the button to be raised up
  while (btnDown()) active_delay(50);
  active_delay(50);  //debounce

  menuOn = 2;

  while (menuOn) {


    ///////////////////////////////  do touch
    if (readTouch()) {
      while (readTouch())active_delay(100);
      scaleTouch(&ts_point);

      if (180 < ts_point.x && ts_point.x < 250 + Menu_Position[0].w &&
          Menu_Position[0].y < ts_point.y && ts_point.y < Menu_Position[0].y + Menu_Position[0].h ) {
        if ( Smtr_Active ) {
          setupSmtrConstant();
        }

      } else if (Menu_Position[0].x < ts_point.x && ts_point.x < Menu_Position[0].x + Menu_Position[0].w &&
                 Menu_Position[0].y < ts_point.y && ts_point.y < Menu_Position[0].y + Menu_Position[0].h ) {
        if ( Smtr_Active )Smtr_Active = false;
        else Smtr_Active = true;
        EEPROM.put(SMTRACTIVE, Smtr_Active);
        if ( Smtr_Active)smeterInterval = 150;
        if ( Smtr_Active)sprintf(c, "S meter ON   %02d", Smtr_Constant);
        else sprintf(c,             "S meter OFF      ");
        tft.setTextSize(2);
        displayText(c, 30, 20, 240, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
      } else if (Menu_Position[1].x < ts_point.x && ts_point.x < Menu_Position[1].x + Menu_Position[1].w &&
                 Menu_Position[1].y < ts_point.y && ts_point.y < Menu_Position[1].y + Menu_Position[1].h ) {
        setCwTone();
        noTone(CW_TONE);
      } else if (Menu_Position[2].x < ts_point.x && ts_point.x < Menu_Position[2].x + Menu_Position[2].w &&
                 Menu_Position[2].y < ts_point.y && ts_point.y < Menu_Position[2].y + Menu_Position[2].h ) {
        setupCwDelay();

      } else if (Menu_Position[3].x < ts_point.x && ts_point.x < Menu_Position[3].x + Menu_Position[3].w &&
                 Menu_Position[3].y < ts_point.y && ts_point.y < Menu_Position[3].y + Menu_Position[3].h ) {
        setupKeyer();
      } else if (Menu_Position[4].x < ts_point.x && ts_point.x < Menu_Position[4].x + Menu_Position[4].w &&
                 Menu_Position[4].y < ts_point.y && ts_point.y < Menu_Position[4].y + Menu_Position[4].h ) {
        tft.setTextSize(2);
        if ( RTC_Clock ) {
          RTC_Clock = false;
          displayText((char*)"Clock Off", 30, 140, 120, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
        } else {
          displayText((char*)"Clock On ", 30, 140, 120, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
          RTC_Clock = true;
        }
        unsigned long temp = RTC_Clock;
        EEPROM.put(RTC_CLOCK, temp);

      } else if (Menu_Position[5].x < ts_point.x && ts_point.x < Menu_Position[5].x + Menu_Position[5].w &&
                 Menu_Position[5].y < ts_point.y && ts_point.y < Menu_Position[5].y + Menu_Position[5].h ) {
        tft.setTextSize(2);
        Region = Region + 1;
        if ( Region > 3 ) Region = 1;
        EEPROM.put(REGION, Region);
        if ( Region == 1 )displayText((char*)"Region 1", 154, 140, 116, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
        else if ( Region == 2 )displayText((char*)"Region 2", 154, 140, 116, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);
        else if ( Region == 3 )displayText((char*)"Region 3", 154, 140, 116, 28, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, true);

      } else if (Menu_Position[6].x < ts_point.x && ts_point.x < Menu_Position[6].x + Menu_Position[6].w &&
                 Menu_Position[6].y < ts_point.y && ts_point.y < Menu_Position[6].y + Menu_Position[6].h ) {
        isUSB = 1;
        setupFreq();
        drawSetupMenu();
      } else if (Menu_Position[7].x < ts_point.x && ts_point.x < Menu_Position[7].x + Menu_Position[7].w &&
                 Menu_Position[7].y < ts_point.y && ts_point.y < Menu_Position[7].y + Menu_Position[7].h ) {
        isUSB = 0;
        setupBFO();
        drawSetupMenu();
      } else if (Menu_Position[8].x < ts_point.x && ts_point.x < Menu_Position[8].x + Menu_Position[8].w &&
                 Menu_Position[8].y < ts_point.y && ts_point.y < Menu_Position[8].y + Menu_Position[8].h ) {
        menuOn = 0;
      }

    }
    delay(100);
  }
  //debounce the button
  while (btnDown()) active_delay(50);
#ifdef KEYBOARD_INCLUDED
  while (keyboard.available()) keyboard.read();
#endif
  active_delay(50);
  guiUpdate();
}

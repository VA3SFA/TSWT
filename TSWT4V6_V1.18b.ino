
char Version_Text[21] = " TSWT4V6 V1.18b";
char Version_Date[21] = "08/28/2020 21:20";
/**
  This source file is under General Public License version 3.

  Derived from:  TSWT4V6 V1.164yb,V1.153kb,V1.61wb(Bills CW update)
  Derived from:  TSWT4V6 V1.17b

  Fixed:
   0. Updated the CAT frequency command to accept any valid frequency and change
      ubitx to the appropriate band before changing the frequency.  This change
      developed by John (K6JDS).

   1. Added call to setFrequency in mode code
   2. Removed smeter whiteout if Smeter off in startTx
   3. Changed WPM to White when changing values.
   4. Added white "L" for lock on display

   Removed "keyboard" support by using conditional compiling.

  This verision uses a built-in Si5351 library
  Most source code are meant to be understood by the compilers and the computers.
  Code that has to be hackable needs to be well understood and properly documented.
  Donald Knuth coined the term Literate Programming to indicate code that is written be
  easily read and understood.

  The Raduino is a small board that includes the Arduin Nano, a TFT display and
  an Si5351a frequency synthesizer. This board is manufactured by HF Signals Electronics Pvt Ltd

  To learn more about Arduino you may visit www.arduino.cc.

  The Arduino works by starts executing the code in a function called setup() and then it
  repeatedly keeps calling loop() forever. All the initialization code is kept in setup()
  and code to continuously sense the tuning knob, the function button, transmit/receive,
  etc is all in the loop() function. If you wish to study the code top down, then scroll
  to the bottom of this file and read your way up.

  Below are the libraries to be included for building the Raduino
  The EEPROM library is used to store settings like the frequency memory, caliberation data, etc.

   The main chip which generates upto three oscillators of various frequencies in the
   Raduino is the Si5351a. To learn more about Si5351a you can download the datasheet
   from www.silabs.com although, strictly speaking it is not a requirment to understand this code.
   Instead, you can look up the Si5351 library written by xxx, yyy. You can download and
   install it from www.url.com to complile this file.
   The Wire.h library is used to talk to the Si5351 and we also declare an instance of
   Si5351 object to control the clocks.
*/

/*
   l :  Adds band markers
*/

#include <Wire.h>
#include <EEPROM.h>
#include "ubitx.h"
#include "nano_gui.h"
#include <TimeLib.h>
#include <Adafruit_ILI9341.h>


#ifdef KEYBOARD_INCLUDED
#include <PS2Keyboard.h>
PS2Keyboard keyboard;

extern void Process_Keyboard( void );
extern int Keyboard_Mode;
extern int KeyIndex;
char Keyboard_Input_Buffer[41];
#endif

extern void   checkCAT();

extern void switchBand(int band, bool Display);
extern void   display_Mode( void );
extern void  displayBand( void );
extern void initBandData( void);
extern void Connect_Interrupts(void);
extern volatile int TimeToSaveFrequency;
extern void RemoveBandUnderline ( void );
extern void drawTxON();
extern void drawTxOFF();
extern void checkButton( void );
extern void getSpdValue(void);
extern void Display_S_Meter();
extern bool Cal_Freq_In_Progress;
int Freq_Index = 4;
unsigned long Freq_Index_Increment[6] = {1000000L, 100000L, 10000L, 1000L, 100L, 10L };

extern bool Smtr_Active;
extern int  VfoaBand;
extern int  VfobBand;
extern int  Current_Band;
extern int  Current_Mode;
extern volatile bool TXON;
extern bool Spd_Active;

extern volatile bool Do_CAT;
extern volatile int cwMode;

extern unsigned long BandFreqVFOa[8];
extern unsigned long BandFreqVFOb[8];
extern int BandModeVFOa[8];
extern int BandModeVFOb[8];

extern volatile int SmeterReadValue, LastSmeter, SMeter_Value, smeterInterval;
extern int Smtr_Constant;

unsigned long LastFreqSaved;

int ritband;
int last_second = 0;
int current_second;

/**
    The main chip which generates upto three oscillators of various frequencies in the
    Raduino is the Si5351a. To learn more about Si5351a you can download the datasheet
    from www.silabs.com although, strictly speaking it is not a requirment to understand this code.

    We no longer use the standard SI5351 library because of its huge overhead due to many unused
    features consuming a lot of program space. Instead of depending on an external library we now use
    Jerry Gaffke's, KE7ER, lightweight standalone mimimalist "si5351bx" routines (see further down the
    code). Here are some defines and declarations used by Jerry's routines:
*/


/**
   We need to carefully pick assignment of pin for various purposes.
   There are two sets of completely programmable pins on the Raduino.
   First, on the top of the board, in line with the LCD connector is an 8-pin connector
   that is largely meant for analog inputs and front-panel control. It has a regulated 5v output,
   ground and six pins. Each of these six pins can be individually programmed
   either as an analog input, a digital input or a digital output.
   The pins are assigned as follows (left to right, display facing you):
        Pin 1 (Violet), A7, SPARE
        Pin 2 (Blue),   A6, KEYER (DATA)
        Pin 3 (Green), +5v
        Pin 4 (Yellow), Gnd
        Pin 5 (Orange), A3, PTT
        Pin 6 (Red),    A2, F BUTTON
        Pin 7 (Brown),  A1, ENC B
        Pin 8 (Black),  A0, ENC A
  Note: A5, A4 are wired to the Si5351 as I2C interface
 *       *
   Though, this can be assigned anyway, for this application of the Arduino, we will make the following
   assignment
   A2 will connect to the PTT line, which is the usually a part of the mic connector
   A3 is connected to a push button that can momentarily ground this line. This will be used for RIT/Bandswitching, etc.
   A6 is to implement a keyer, it is reserved and not yet implemented
   A7 is connected to a center pin of good quality 100K or 10K linear potentiometer with the two other ends connected to
   ground and +5v lines available on the connector. This implments the tuning mechanism
*/

extern bool Lck_Active;

#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

/** pin assignments
  14  T_IRQ           2 std   changed
  13  T_DOUT              (parallel to SOD/MOSI, pin 9 of display)
  12  T_DIN               (parallel to SDI/MISO, pin 6 of display)
  11  T_CS            9   (we need to specify this)
  10  T_CLK               (parallel to SCK, pin 7 of display)
  9   SDO(MSIO) 12    12  (spi)
  8   LED       A0    8   (not needed, permanently on +3.3v) (resistor from 5v,
  7   SCK       13    13  (spi)
  6   SDI       11    11  (spi)
  5   D/C       A3    7   (changable)
  4   RESET     A4    9 (not needed, permanently +5v)
  3   CS        A5    10  (changable)
  2   GND       GND
  1   VCC       VCC

  The model is called tjctm24028-spi
  it uses an ILI9341 display controller and an  XPT2046 touch controller.
*/

#define TFT_DC  9
#define TFT_CS 10

#define CS_PIN  8



struct Point ts_point;



/**
   The Arduino, unlike C/C++ on a regular computer with gigabytes of RAM, has very little memory.
   We have to be very careful with variables that are declared inside the functions as they are
   created in a memory region called the stack. The stack has just a few bytes of space on the Arduino
   if you declare large strings inside functions, they can easily exceed the capacity of the stack
   and mess up your programs.
   We circumvent this by declaring a few global buffers as  kitchen counters where we can
   slice and dice our strings. These strings are mostly used to control the display or handle
   the input and output from the USB port. We must keep a count of the bytes used while reading
   the serial port as we can easily run out of buffer space. This is done in the serial_in_count variable.
*/
char c[30], b[30];
char printBuff[2][20];  //mirrors what is showing on the two lines of the display
int count = 0;          //to generally count ticks, loops, etc
/**
    The second set of 16 pins on the Raduino's bottom connector are have the three clock outputs and the digital lines to control the rig.
    This assignment is as follows :
      Pin   1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16
           GND +5V CLK0  GND  GND  CLK1 GND  GND  CLK2  GND  D2   D3   D4   D5   D6   D7
    These too are flexible with what you may do with them, for the Raduino, we use them to :
    - TX_RX line : Switches between Transmit and Receive after sensing the PTT or the morse keyer
    - CW_KEY line : turns on the carrier for CW
*/

#define TX_RX (7)
#define CW_TONE (6)
#define TX_LPF_A (5)
#define TX_LPF_B (4)
#define TX_LPF_C (3)
#define CW_KEY (2)


/**
   The uBITX is an upconnversion transceiver. The first IF is at 45 MHz.
   The first IF frequency is not exactly at 45 Mhz but about 5 khz lower,
   this shift is due to the loading on the 45 Mhz crystal filter by the matching
   L-network used on it's either sides.
   The first oscillator works between 48 Mhz and 75 MHz. The signal is subtracted
   from the first oscillator to arriive at 45 Mhz IF. Thus, it is inverted : LSB becomes USB
   and USB becomes LSB.
   The second IF of 12 Mhz has a ladder crystal filter. If a second oscillator is used at
   57 Mhz, the signal is subtracted FROM the oscillator, inverting a second time, and arrives
   at the 12 Mhz ladder filter thus doouble inversion, keeps the sidebands as they originally were.
   If the second oscillator is at 33 Mhz, the oscilaltor is subtracated from the signal,
   thus keeping the signal's sidebands inverted. The USB will become LSB.
   We use this technique to switch sidebands. This is to avoid placing the lsbCarrier close to
   12 MHz where its fifth harmonic beats with the arduino's 16 Mhz oscillator's fourth harmonic
*/


#define INIT_USB_FREQ   (11059200l)

//we directly generate the CW by programmin the Si5351 to the cw tx frequency, hence, both are different modes
//these are the parameter passed to startTx
#define TX_SSB 0
#define TX_CW 1

char ritOn = 0;
char vfoActive = VFO_A;

unsigned long  sideTone = 800, usbCarrier;
char isUsbVfoA = 0, isUsbVfoB = 1;
unsigned long frequency, ritRxFrequency, ritTxFrequency;  //frequency is the current frequency on the dial
unsigned long firstIF =   45005000L;

char isUSB = 0;               //upper sideband was selected, this is reset to the default for the


//these are variables that control the keyer behaviour
int cwSpeed = 100; //this is actuall the dot period in milliseconds
int wpm = 50;
extern int32_t calibration;
int cwDelayTime = 60;

volatile  CW_PORT CW_Port_Type;
#define IAMBICB 0x10 // 0 for Iambic A, 1 for Iambic B
volatile unsigned char keyerControl = IAMBICB;


/**
   Raduino needs to keep track of current state of the transceiver. These are a few variables that do it
*/
volatile bool inTx = false;                //it is set to 1 if in transmit mode (whatever the reason : cw, ptt or cat)
int splitOn = 0;             //working split, uses VFO B as the transmit frequency
volatile bool keyDown = 0;             //in cw mode, denotes the carrier is being transmitted
//frequency when it crosses the frequency border of 10 MHz
byte menuOn = 0;              //set to 1 when the menu is being displayed, if a menu item sets it to zero, the menu is exited
unsigned long cwTimeout = 0;  //milliseconds to go before the cw transmit line is released and the radio goes back to rx mode

/**
   Below are the basic functions that control the uBitx. Understanding the functions before
   you start hacking around
*/
bool RTC_Clock = false;



void Display_Time( void) {

  if (!cwMode)noInterrupts();
  tft.drawRect(0, 10, 100, 22, DISPLAY_YELLOW);
  sprintf(c, "%02d:%02d:%02d", hour(), minute(), second());
  tft.setCursor(2, 14);
  tft.setTextSize(2);
  tft.setTextColor(DISPLAY_YELLOW, DISPLAY_BLUE);
  tft.print(c);
  tft.setTextSize(3);
  if (!cwMode)interrupts();

}





/**
   Our own delay. During any delay, the raduino should still be processing a few times.
*/

void active_delay(int delay_by) {
  unsigned long timeStart = millis();
  while (millis() - timeStart <= (unsigned long)delay_by) {
    delay(10);
  }
}

/**
   Select the properly tx harmonic filters
   The four harmonic filters use only three relays
   the four LPFs cover 30-21 Mhz, 18 - 14 Mhz, 7-10 MHz and 3.5 to 5 Mhz
   Briefly, it works like this,
   - When KT1 is OFF, the 'off' position routes the PA output through the 30 MHz LPF
   - When KT1 is ON, it routes the PA output to KT2. Which is why you will see that
     the KT1 is on for the three other cases.
   - When the KT1 is ON and KT2 is off, the off position of KT2 routes the PA output
     to 18 MHz LPF (That also works for 14 Mhz)
   - When KT1 is On, KT2 is On, it routes the PA output to KT3
   - KT3, when switched on selects the 7-10 Mhz filter
   - KT3 when switched off selects the 3.5-5 Mhz filter
   See the circuit to understand this
*/

void setTXFilters(unsigned long freq) {

  if (freq > 21000000L) { // the default filter is with 35 MHz cut-off
    digitalWrite(TX_LPF_A, 0);
    digitalWrite(TX_LPF_B, 0);
    digitalWrite(TX_LPF_C, 0);
  }
  else if (freq >= 14000000L) { //thrown the KT1 relay on, the 30 MHz LPF is bypassed and the 14-18 MHz LPF is allowd to go through
    digitalWrite(TX_LPF_A, 1);
    digitalWrite(TX_LPF_B, 0);
    digitalWrite(TX_LPF_C, 0);
  }
  else if (freq > 7000000L) {
    digitalWrite(TX_LPF_A, 0);
    digitalWrite(TX_LPF_B, 1);
    digitalWrite(TX_LPF_C, 0);
  }
  else {
    digitalWrite(TX_LPF_A, 0);
    digitalWrite(TX_LPF_B, 0);
    digitalWrite(TX_LPF_C, 1);
  }
}


void setTXFilters_v5(unsigned long freq) {

  if (freq > 21000000L) { // the default filter is with 35 MHz cut-off
    digitalWrite(TX_LPF_A, 0);
    digitalWrite(TX_LPF_B, 0);
    digitalWrite(TX_LPF_C, 0);
  }
  else if (freq >= 14000000L) { //thrown the KT1 relay on, the 30 MHz LPF is bypassed and the 14-18 MHz LPF is allowd to go through
    digitalWrite(TX_LPF_A, 1);
    digitalWrite(TX_LPF_B, 0);
    digitalWrite(TX_LPF_C, 0);
  }
  else if (freq > 7000000L) {
    digitalWrite(TX_LPF_A, 0);
    digitalWrite(TX_LPF_B, 1);
    digitalWrite(TX_LPF_C, 0);
  }
  else {
    digitalWrite(TX_LPF_A, 0);
    digitalWrite(TX_LPF_B, 0);
    digitalWrite(TX_LPF_C, 1);
  }
}


/**
   This is the most frequently called function that configures the
   radio to a particular frequeny, sideband and sets up the transmit filters

   The transmit filter relays are powered up only during the tx so they dont
   draw any current during rx.

   The carrier oscillator of the detector/modulator is permanently fixed at
   uppper sideband. The sideband selection is done by placing the second oscillator
   either 12 Mhz below or above the 45 Mhz signal thereby inverting the sidebands
   through mixing of the second local oscillator.
*/

void setFrequency(unsigned long f) {
  unsigned long Local_Freq;
  unsigned long Local_usbCarrier;

  Local_usbCarrier = usbCarrier;
  Local_Freq = f;

  if ( !inTx ) {
    Local_usbCarrier = usbCarrier;
    Local_Freq = f;
  }

  setTXFilters(Local_Freq);

  //alternative to reduce the intermod spur
  if (isUSB) {
    if ((cwMode) && (!inTx)) si5351bx_setfreq(2, firstIF  + Local_Freq - sideTone);
    else                     si5351bx_setfreq(2, firstIF  + Local_Freq );

    si5351bx_setfreq(1, firstIF + Local_usbCarrier );
  }
  else {
    if ((cwMode) && (!inTx)) si5351bx_setfreq(2, firstIF  + Local_Freq + sideTone);
    else                     si5351bx_setfreq(2, firstIF  + Local_Freq);

    si5351bx_setfreq(1, firstIF - Local_usbCarrier );
  }


  frequency = f;

}

/**
   startTx is called by the PTT, cw keyer and CAT protocol to
   put the uBitx in tx mode. It takes care of rit settings, sideband settings
   Note: In cw mode, doesnt key the radio, only puts it in tx mode
   CW offest is calculated as lower than the operating frequency when in LSB mode, and vice versa in USB mode
*/

void startTx(byte txMode) {

  if ( TXON ) {
    inTx = true;
    digitalWrite(TX_RX, 1);
    if ( Smtr_Active == true  )tft.fillRect(130, 22, 184, 6, ILI9341_WHITE);

    if (ritOn) {
      //save the current as the rx frequency
      ritRxFrequency = frequency;
      setFrequency(ritTxFrequency);
    } else {
      if (splitOn == 1) {
        if (vfoActive == VFO_B) {
          vfoActive = VFO_A;
          isUSB = isUsbVfoA;
          frequency = BandFreqVFOa[VfoaBand];
        } else if (vfoActive == VFO_A) {
          vfoActive = VFO_B;
          frequency = BandFreqVFOb[VfobBand];
          isUSB = isUsbVfoB;
        }
      }
      setFrequency(frequency);
    }

    if (txMode == TX_CW) {
      digitalWrite(TX_RX, 0);

      //turn off the second local oscillator and the bfo
      si5351bx_setfreq(0, 0);
      si5351bx_setfreq(1, 0);
      si5351bx_setfreq(2, frequency );
      digitalWrite(TX_RX, 1);
    }
    drawTxON();
  }
}

void stopTx() {

  if ( !Do_CAT ) {

    inTx = false;
    LastSmeter = 0;
    digitalWrite(TX_RX, 0);           //turn off the tx
    si5351bx_setfreq(0, usbCarrier);  //set back the cardrier oscillator anyway, cw tx switches it off

    if (ritOn)
      setFrequency(ritRxFrequency);
    else {
      if (splitOn == 1) {
        //vfo Change
        if (vfoActive == VFO_B) {
          vfoActive = VFO_A;
          frequency = BandFreqVFOa[VfoaBand];
          isUSB = isUsbVfoA;
        }
        else if (vfoActive == VFO_A) {
          vfoActive = VFO_B;
          frequency = BandFreqVFOb[VfobBand];
          isUSB = isUsbVfoB;
        }
      }
      setFrequency(frequency);
    }
  }
  drawTxOFF();
}

/**
   ritEnable is called with a frequency parameter that determines
   what the tx frequency will be
*/
void ritEnable(unsigned long f) {
  ritOn = 1;
  //save the non-rit frequency back into the VFO memory
  //as RIT is a temporary shift, this is not saved to EEPROM
  ritTxFrequency = f;
  ritband = Current_Band;
}

// this is called by the RIT menu routine
void ritDisable() {
  if (ritOn) {
    ritOn = 0;
    if ( ritband != Current_Band )switchBand(ritband, true);
    setFrequency(ritTxFrequency);
    updateDisplay();
  }
}

/**
   Basic User Interface Routines. These check the front panel for any activity
*/

/**
   The PTT is checked only if we are not already in a cw transmit session
   If the PTT is pressed, we shift to the ritbase if the rit was on
   flip the T/R line to T and update the display to denote transmission
*/
void getSpdValue(void) {
  int knob = enc_read();
  if ( knob != 0) {
    if (wpm > 5 && knob < 0)  wpm = wpm - 1;
    else if (wpm < 75 && knob > 0) wpm = wpm + 1;
    cwSpeed = 600 / wpm;
    EEPROM.put(CW_SPEED, cwSpeed);
    sprintf(c, "%02d", wpm);
    tft.setTextSize(3);
    displayText(c, 240, 38, 44, 33, DISPLAY_WHITE, DISPLAY_BLUE, DISPLAY_BLUE, false);
    tft.setTextSize(2);
    tft.setCursor(280, 52);
    tft.print("wpm");
  }
}


void Set_Mode ( void ) {
  TXON = true;
  switch ( Current_Mode ) {
    case 0:
      isUSB = 1;
      cwMode = 0;
      break;
    case 1:
      isUSB = 0;
      cwMode = 0;
      break;
    case 2:
      isUSB = 1;
      cwMode = 1;
      break;
    case 3:
      isUSB = 0;
      cwMode = 1;
      break;
    case 4:
      isUSB = 1;
      cwMode = 1;
      TXON = false;
      break;
  }
  setFrequency(frequency);
}

void switchVFO(int vfoSelect, bool Display) {


  if (vfoSelect == VFO_A) {
    if (vfoActive == VFO_B) {
      BandFreqVFOb[VfobBand] = frequency;
      EEPROM.put(BANDFREQVFOb_80 + (4 * VfobBand), frequency);
      EEPROM.put(BANDMODEVFOb_80 + (4 * VfobBand), Current_Mode);
    }
    Current_Band = VfoaBand;
    vfoActive = VFO_A;
    EEPROM.get(BANDFREQVFOa_80 + (4 * VfoaBand), frequency);
    EEPROM.get(BANDMODEVFOa_80 + (4 * VfoaBand), Current_Mode);
  } else {
    if (vfoActive == VFO_A) {
      BandFreqVFOa[VfoaBand] = frequency;
      EEPROM.put(BANDFREQVFOa_80 + (4 * VfoaBand), frequency);
      EEPROM.put(BANDMODEVFOa_80 + (4 * VfoaBand), Current_Mode);
    }
    Current_Band = VfobBand;
    vfoActive = VFO_B;
    EEPROM.get(BANDFREQVFOb_80 + (4 * VfobBand), frequency);
    EEPROM.get(BANDMODEVFOb_80 + (4 * VfobBand), Current_Mode);
  }
  setFrequency(frequency);
  Set_Mode();
  display_Mode();
  displayBand();
  EEPROM.put( SELECTED_VFO, vfoActive);
  if ( Display )redrawVFOs();

}


/**
   The tuning jumps by 50 Hz on each step when you tune slowly
   As you spin the encoder faster, the jump size also increases
   This way, you can quickly move to another band by just spinning the
   tuning knob
*/
int Region;
unsigned long Band_Low_Limits[3][8] = {
  3500000L, 7000000L, 10100000L, 14000000L,
  18068000L, 21000000L, 28000000L, 100000L,

  3500000L, 7000000L, 10100000L, 14000000L,
  18068000L, 21000000L, 28000000L, 100000L,

  3500000L, 7000000L, 10110000L, 14000000L,
  18068000L, 21000000L, 28000000L, 100000L
};


unsigned long Band_High_Limits[3][8] = {
  3800000L, 7200000L, 10150000L, 14350000L,
  18168000L, 21450000L, 29700000L, 40000000L,

  4000000L, 7300000L, 10150000L, 14350000L,
  18168000L, 21450000L, 29700000L, 40000000L,

  3900000L, 7200000L, 10150000L, 14350000L,
  18168000L, 21450000L, 29700000L, 40000000L
};

void Encoder_Update_Freq(int s) {

  if (s > 0) {
    frequency = frequency + Freq_Index_Increment[Freq_Index];
    if ( frequency > Band_High_Limits[Region - 1][Current_Band])
      frequency = Band_High_Limits[Region - 1][Current_Band];
  }
  else if (s  < 0) {
    if (frequency <= Freq_Index_Increment[Freq_Index])
      frequency = Band_Low_Limits[Region - 1][Current_Band];
    else {
      frequency = frequency - Freq_Index_Increment[Freq_Index];
      if ( frequency < Band_Low_Limits[Region - 1][Current_Band])
        frequency = Band_Low_Limits[Region - 1][Current_Band];
    }
  }

  setFrequency(frequency);

  updateDisplay();
  TimeToSaveFrequency = TimeUntilSave_Frequency;
}

void doTuning() {
  int s = 1;

  if ( Lck_Active == false ) {
    s = enc_read();
    if ( s != 0) {
      Encoder_Update_Freq(s);
      delay(150);
      s = enc_read();
      while ( s != 0 ) {
        Encoder_Update_Freq(s);
        s = enc_read();
      }
    }
  }
}

/**
   RIT only steps back and forth by 100 hz at a time
*/
void doRIT() {

  int s = enc_read();
  unsigned long old_freq = frequency;

  if (s > 0) {
    frequency = frequency + Freq_Index_Increment[Freq_Index];
    if ( frequency > Band_High_Limits[Region - 1][Current_Band])
      frequency = Band_High_Limits[Region - 1][Current_Band];
  }
  else if (s  < 0) {
    if (frequency <= Freq_Index_Increment[Freq_Index])
      frequency = Band_Low_Limits[Region - 1][Current_Band];
    else {
      frequency = frequency - Freq_Index_Increment[Freq_Index];
      if ( frequency < Band_Low_Limits[Region - 1][Current_Band])
        frequency = Band_Low_Limits[Region - 1][Current_Band];
    }
  }

  if (old_freq != frequency) {
    setFrequency(frequency);
    updateDisplay();
  }
}

/**
   The settings are read from EEPROM. The first time around, the values may not be
   present or out of range, in this case, some intelligent defaults are copied into the
   variables.
*/
void initSettings() {

  //read the settings from the eeprom and restore them
  //if the readings are off, then set defaults
  EEPROM.get(MASTER_CAL, calibration);

  EEPROM.get(USB_CAL, usbCarrier);

  EEPROM.get(SMTRACTIVE, Smtr_Active);
  if ( (Smtr_Active < 0) || (Smtr_Active > 1))Smtr_Active = 0;

  EEPROM.get(SMTRCONSTANT, Smtr_Constant);
  if ( (Smtr_Constant <= 0) || (Smtr_Constant > 100) )Smtr_Constant = 24;

  EEPROM.get(CW_SIDETONE, sideTone);

  EEPROM.get(CW_SPEED, cwSpeed);
  wpm = 600 / cwSpeed;
  if (wpm >= 75 ) {
    cwSpeed = 600 / 50;
    EEPROM.put(CW_SPEED, cwSpeed);
  } else if (wpm < 5) {
    cwSpeed = 600 / 50;
    EEPROM.put(CW_SPEED, cwSpeed);
  }
  wpm = 600 / cwSpeed;

  EEPROM.get(CW_DELAYTIME, cwDelayTime);
  if ((cwDelayTime < 10) || (cwDelayTime > 1000) ) {
    cwDelayTime = 500;
    EEPROM.put(CW_DELAYTIME, cwDelayTime);
  }


  if (usbCarrier > 11060000l || usbCarrier < 11048000l)
    usbCarrier = 11052000l;
  if (sideTone < 100 || 2000 < sideTone)
    sideTone = 800;




  /*
     The keyer type splits into two variables
  */
  unsigned long temp;
  EEPROM.get(CW_KEY_TYPE, temp);
  CW_Port_Type = CW_PORT(temp);

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

  EEPROM.get(RTC_CLOCK, temp);
  RTC_Clock = temp;

  EEPROM.get(SELECTED_FREQ_INDEX, Freq_Index);
  if ( (Freq_Index < 0) || (Freq_Index > 5)) {
    Freq_Index = 2;
    EEPROM.put(SELECTED_FREQ_INDEX, Freq_Index);
  }

  EEPROM.get(REGION, temp);
  Region = temp;
  if ( (Region < 1) || (Region > 3)) {
    Region = 1;
    temp = 1;
    EEPROM.put(REGION, Region);
  }

}

void initPorts() {

  pinMode(SMETERREADPIN, INPUT);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(FBUTTON, INPUT_PULLUP);
  pinMode(PTT, INPUT_PULLUP);
  pinMode(DIGITAL_DOT, INPUT_PULLUP);
  pinMode(DIGITAL_DASH, INPUT_PULLUP);
  pinMode(CW_TONE, OUTPUT);
  digitalWrite(CW_TONE, 0);

  pinMode(TX_RX, OUTPUT);
  digitalWrite(TX_RX, 0);

  pinMode(TX_LPF_A, OUTPUT);
  pinMode(TX_LPF_B, OUTPUT);
  pinMode(TX_LPF_C, OUTPUT);
  digitalWrite(TX_LPF_A, 0);
  digitalWrite(TX_LPF_B, 0);
  digitalWrite(TX_LPF_C, 0);

  pinMode(CW_KEY, OUTPUT);
  digitalWrite(CW_KEY, 0);
}


time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}

void setup()
{
  // set the Time library to use Teensy 3.0's RTC to keep time
  setSyncProvider(getTeensy3Time);

  Serial.begin(38400);
  tft.begin();
  tft.begin();
  tft.setRotation(1);
  tft.setTextSize(4);
  tft.fillScreen(DISPLAY_WHITE);
  tft.setTextColor(DISPLAY_RED);
  tft.println("\n     TSW\n");
  tft.setTextSize(3);
  tft.setTextColor(DISPLAY_GREEN);
  tft.println("  W0EB N5IB W2CTX");
  tft.setTextColor(DISPLAY_RED);
  tft.println("   www.w0eb.com\n");
  tft.setTextColor(DISPLAY_BLUE);
  sprintf(c, " %s", Version_Text);
  tft.println(c);
  sprintf(c, " %s", Version_Date);
  tft.print(c);

  delay(2000);


  initSettings();
  initPorts();
  displayInit();
  initOscillators();
  initBandData();

  // get last selected SELECTED_VFO
  EEPROM.get( SELECTED_VFO, vfoActive);
  if ( (vfoActive != VFO_A) && (vfoActive != VFO_B))vfoActive = VFO_A;
  if ( vfoActive == VFO_A ) {
    Current_Band = VfoaBand;
    Current_Mode = BandModeVFOa[Current_Band];
    frequency =    BandFreqVFOa[Current_Band];
  } else {
    Current_Band = VfobBand;
    Current_Mode = BandModeVFOb[Current_Band];
    frequency =    BandFreqVFOb[Current_Band];
  }
  Set_Mode();
  setFrequency(frequency);



  if (btnDown()) {
    tft.fillScreen(DISPLAY_BLACK);
    setupTouch();
  }
  guiUpdate();
  Connect_Interrupts();
  while ( enc_read() != 0 ) delay(100);;

}


/**
   The loop checks for keydown, ptt, function button and tuning.
*/

byte flasher = 0;
boolean wastouched = false;

void loop() {

  //Check Serial Port Buffer
  checkCAT();

  //tune only when not tranmsitting
  if (!inTx) {

#ifdef KEYBOARD_INCLUDED
    // do only of keyboard not active
    if ( (Keyboard_Mode == KEY_IDLE) || (Keyboard_Mode == KEY_CW) ) {

      if (ritOn && !Spd_Active && !Lck_Active) doRIT();
      else if (Spd_Active ) getSpdValue();
      else doTuning();
    }
    if ( Keyboard_Mode == KEY_IDLE ) {

      checkButton();
      checkTouch();
    }
#else
    if (ritOn && !Spd_Active && !Lck_Active) doRIT();
    else if (Spd_Active ) getSpdValue();
    else doTuning();
    checkButton();
    checkTouch();
#endif

    if ( TimeToSaveFrequency == 0 ) {
      TimeToSaveFrequency = -1;
      if (vfoActive == VFO_A) {
        EEPROM.put(BANDFREQVFOa_80 + (4 * Current_Band), frequency);
        BandFreqVFOa[Current_Band] = frequency;
        BandModeVFOa[Current_Band] = Current_Mode;
        EEPROM.put(BANDMODEVFOa_80 + (4 * Current_Band), Current_Mode);

      } else {
        EEPROM.put(BANDFREQVFOb_80 + (4 * Current_Band), frequency);
        BandFreqVFOb[Current_Band] = frequency;
        BandModeVFOb[Current_Band] = Current_Mode;
        EEPROM.put(BANDMODEVFOb_80 + (4 * Current_Band), Current_Mode);
      }
    }
    if ( Smtr_Active == true ) {
      if ( smeterInterval <= 0 ) {
        Display_S_Meter();
        smeterInterval = 150;
      }
    }
  }

#ifdef KEYBOARD_INCLUDED
  // process keyboard
  if ( CW_Port_Type == KEYBOARD ) Process_Keyboard();
#endif

  // process clock
  if ( RTC_Clock ) {
    current_second = second();
    if ( current_second != last_second ) {
      last_second = current_second;
      Display_Time();
    }
  }
}

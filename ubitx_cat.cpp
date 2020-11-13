#include <Arduino.h>
#include "ubitx.h"
#include "nano_gui.h"
#include <TimeLib.h>
#include <Adafruit_ILI9341.h>

extern volatile bool Turn_Off_Carrier_in_Progress;
extern volatile unsigned long  Turn_Off_Carrier_Timer_Count;
extern volatile bool PTT_HANDKEY_ACTIVE;
extern unsigned long Local_Tx_Delay;
extern volatile long last_interrupt_time;
extern void switchBand(int band, bool Display);
extern void  display_Mode( void );

extern Adafruit_ILI9341 tft;

extern volatile bool Do_CAT;
extern volatile int cwMode;
extern unsigned long Band_Low_Limits[3][8];
extern unsigned long Band_High_Limits[3][8];
extern int  Current_Band;
extern void Print_Debug( char *text, int val);
extern int Region;
extern int BandModeVFOa[8];
extern int BandModeVFOb[8];

extern volatile int LastSmeter;

extern volatile bool TXON;
extern int Current_Mode;
extern void Set_Mode( void);

volatile bool Saved_TXON;
int Saved_Mode;
/**
   The CAT protocol is used by many radios to provide remote control to comptuers through
   the serial port.

   This is very much a work in progress. Parts of this code have been liberally
   borrowed from other GPLicensed works like hamlib.

   WARNING : This is an unstable version and it has worked with fldigi,
   it gives time out error with WSJTX 1.8.0
*/

extern void local_rtc_set(unsigned long t);
extern void cwKeydown();
extern void cwKeyUp();

static unsigned long rxBufferArriveTime = 0;
volatile  byte rxBufferCheckCount = 0;
#define CAT_RECEIVE_TIMEOUT 500
static byte cat[24];
static byte insideCat = 0;

//for broken protocol
#define CAT_RECEIVE_TIMEOUT 500

#define CAT_MODE_LSB            0x00
#define CAT_MODE_USB            0x01
#define CAT_MODE_CW             0x02
#define CAT_MODE_CWR            0x03
#define CAT_MODE_AM             0x04
#define CAT_MODE_FM             0x08
#define CAT_MODE_DIG            0x0A
#define CAT_MODE_PKT            0x0C
#define CAT_MODE_FMN            0x88

#define ACK 0

unsigned int skipTimeCount = 0;




// code provided by K6JDS
int getBand(unsigned long freq) {
  int i;
  int band = -1;

  for (i = 0; i < 8; i++) {
    if ( freq <= Band_High_Limits[Region - 1][i] && freq >= Band_Low_Limits[Region - 1][i]) {
      band = i;
      break;
    }
  }
  return band;
}



byte setHighNibble(byte b, byte v) {
  // Clear the high nibble
  b &= 0x0f;
  // Set the high nibble
  return b | ((v & 0x0f) << 4);
}

byte setLowNibble(byte b, byte v) {
  // Clear the low nibble
  b &= 0xf0;
  // Set the low nibble
  return b | (v & 0x0f);
}

byte getHighNibble(byte b) {
  return (b >> 4) & 0x0f;
}

byte getLowNibble(byte b) {
  return b & 0x0f;
}

// Takes a number and produces the requested number of decimal digits, staring
// from the least significant digit.
//
void getDecimalDigits(unsigned long number, byte* result, int digits) {
  for (int i = 0; i < digits; i++) {
    // "Mask off" (in a decimal sense) the LSD and return it
    result[i] = number % 10;
    // "Shift right" (in a decimal sense)
    number /= 10;
  }
}

// Takes a frequency and writes it into the CAT command buffer in BCD form.
//
void writeFreq(unsigned long freq, byte* cmd) {
  // Convert the frequency to a set of decimal digits. We are taking 9 digits
  // so that we can get up to 999 MHz. But the protocol doesn't care about the
  // LSD (1's place), so we ignore that digit.
  byte digits[9];
  getDecimalDigits(freq, digits, 9);
  // Start from the LSB and get each nibble
  cmd[3] = setLowNibble(cmd[3], digits[1]);
  cmd[3] = setHighNibble(cmd[3], digits[2]);
  cmd[2] = setLowNibble(cmd[2], digits[3]);
  cmd[2] = setHighNibble(cmd[2], digits[4]);
  cmd[1] = setLowNibble(cmd[1], digits[5]);
  cmd[1] = setHighNibble(cmd[1], digits[6]);
  cmd[0] = setLowNibble(cmd[0], digits[7]);
  cmd[0] = setHighNibble(cmd[0], digits[8]);
}

// This function takes a frquency that is encoded using 4 bytes of BCD
// representation and turns it into an long measured in Hz.
//
// [12][34][56][78] = 123.45678? Mhz
//
unsigned long readFreq(byte* cmd) {
  // Pull off each of the digits
  byte d7 = getHighNibble(cmd[0]);
  byte d6 = getLowNibble(cmd[0]);
  byte d5 = getHighNibble(cmd[1]);
  byte d4 = getLowNibble(cmd[1]);
  byte d3 = getHighNibble(cmd[2]);
  byte d2 = getLowNibble(cmd[2]);
  byte d1 = getHighNibble(cmd[3]);
  byte d0 = getLowNibble(cmd[3]);
  return
    (unsigned long)d7 * 100000000L +
    (unsigned long)d6 * 10000000L +
    (unsigned long)d5 * 1000000L +
    (unsigned long)d4 * 100000L +
    (unsigned long)d3 * 10000L +
    (unsigned long)d2 * 1000L +
    (unsigned long)d1 * 100L +
    (unsigned long)d0 * 10L;
}

//void ReadEEPRom_FT817(byte fromType)
void catReadEEPRom(void)
{
  //for remove warnings
  byte temp0 = cat[0];
  byte temp1 = cat[1];
  /*
    itoa((int) cat[0], b, 16);
    strcat(b, ":");
    itoa((int) cat[1], c, 16);
    strcat(b, c);
    printLine2(b);
  */

  cat[0] = 0;
  cat[1] = 0;
  //for remove warnings[1] = 0;

  switch (temp1)
  {
    case 0x45 : //
      if (temp0 == 0x03)
      {
        cat[0] = 0x00;
        cat[1] = 0xD0;
      }
      break;
    case 0x47 : //
      if (temp0 == 0x03)
      {
        cat[0] = 0xDC;
        cat[1] = 0xE0;
      }
      break;
    case 0x55 :
      //0 : VFO A/B  0 = VFO-A, 1 = VFO-B
      //1 : MTQMB Select  0 = (Not MTQMB), 1 = MTQMB ("Memory Tune Quick Memory Bank")
      //2 : QMB Select  0 = (Not QMB), 1 = QMB  ("Quick Memory Bank")
      //3 :
      //4 : Home Select  0 = (Not HOME), 1 = HOME memory
      //5 : Memory/MTUNE select  0 = Memory, 1 = MTUNE
      //6 :
      //7 : MEM/VFO Select  0 = Memory, 1 = VFO (A or B - see bit 0)
      cat[0] = 0x80 + (vfoActive == VFO_B ? 1 : 0);
      cat[1] = 0x00;
      break;
    case 0x57 : //
      //0 : 1-0  AGC Mode  00 = Auto, 01 = Fast, 10 = Slow, 11 = Off
      //2  DSP On/Off  0 = Off, 1 = On  (Display format)
      //4  PBT On/Off  0 = Off, 1 = On  (Passband Tuning)
      //5  NB On/Off 0 = Off, 1 = On  (Noise Blanker)
      //6  Lock On/Off 0 = Off, 1 = On  (Dial Lock)
      //7  FST (Fast Tuning) On/Off  0 = Off, 1 = On  (Fast tuning)

      cat[0] = 0xC0;
      cat[1] = 0x40;
      break;
    case 0x59 : //  band select VFO A Band Select  0000 = 160 M, 0001 = 75 M, 0010 = 40 M, 0011 = 30 M, 0100 = 20 M, 0101 = 17 M, 0110 = 15 M, 0111 = 12 M, 1000 = 10 M, 1001 = 6 M, 1010 = FM BCB, 1011 = Air, 1100 = 2 M, 1101 = UHF, 1110 = (Phantom)
      //http://www.ka7oei.com/ft817_memmap.html
      //CAT_BUFF[0] = 0xC2;
      //CAT_BUFF[1] = 0x82;
      break;
    case 0x5C : //Beep Volume (0-100) (#13)
      cat[0] = 0xB2;
      cat[1] = 0x42;
      break;
    case 0x5E :
      //3-0 : CW Pitch (300-1000 Hz) (#20)  From 0 to E (HEX) with 0 = 300 Hz and each step representing 50 Hz
      //5-4 :  Lock Mode (#32) 00 = Dial, 01 = Freq, 10 = Panel
      //7-6 :  Op Filter (#38) 00 = Off, 01 = SSB, 10 = CW
      //CAT_BUFF[0] = 0x08;
      cat[0] = (sideTone - 300) / 50;
      cat[1] = 0x25;
      break;
    case 0x61 : //Sidetone (Volume) (#44)
      cat[0] = sideTone % 50;
      cat[1] = 0x08;
      break;
    case  0x5F : //
      //4-0  CW Weight (1.:2.5-1:4.5) (#22)  From 0 to 14 (HEX) with 0 = 1:2.5, incrementing in 0.1 weight steps
      //5  420 ARS (#2)  0 = Off, 1 = On
      //6  144 ARS (#1)  0 = Off, 1 = On
      //7  Sql/RF-G (#45)  0 = Off, 1 = On
      cat[0] = 0x32;
      cat[1] = 0x08;
      break;
    case 0x60 : //CW Delay (10-2500 ms) (#17)  From 1 to 250 (decimal) with each step representing 10 ms
      cat[0] = cwDelayTime;
      cat[1] = 0x32;
      break;
    case 0x62 : //
      //5-0  CW Speed (4-60 WPM) (#21) From 0 to 38 (HEX) with 0 = 4 WPM and 38 = 60 WPM (1 WPM steps)
      //7-6  Batt-Chg (6/8/10 Hours (#11)  00 = 6 Hours, 01 = 8 Hours, 10 = 10 Hours
      //CAT_BUFF[0] = 0x08;
      cat[0] = 600 / cwSpeed - 4;
      cat[1] = 0xB2;
      break;
    case 0x63 : //
      //6-0  VOX Gain (#51)  Contains 1-100 (decimal) as displayed
      //7  Disable AM/FM Dial (#4) 0 = Enable, 1 = Disable
      cat[0] = 0xB2;
      cat[1] = 0xA5;
      break;
    case 0x64 : //
      break;
    case 0x67 : //6-0  SSB Mic (#46) Contains 0-100 (decimal) as displayed
      cat[0] = 0xB2;
      cat[1] = 0xB2;
      break;
    case 0x69 : //FM Mic (#29)  Contains 0-100 (decimal) as displayed
      break;
    case 0x78 :
      if (isUSB)
        cat[0] = CAT_MODE_USB;
      else
        cat[0] = CAT_MODE_LSB;

      if (cat[0] != 0) cat[0] = 1 << 5;
      break;
    case  0x79 : //
      //1-0  TX Power (All bands)  00 = High, 01 = L3, 10 = L2, 11 = L1
      //3  PRI On/Off  0 = Off, 1 = On
      //DW On/Off  0 = Off, 1 = On
      //SCN (Scan) Mode  00 = No scan, 10 = Scan up, 11 = Scan down
      //ART On/Off  0 = Off, 1 = On
      cat[0] = 0x00;
      cat[1] = 0x00;
      break;
    case 0x7A : //SPLIT
      //7A  0 HF Antenna Select 0 = Front, 1 = Rear
      //7A  1 6 M Antenna Select  0 = Front, 1 = Rear
      //7A  2 FM BCB Antenna Select 0 = Front, 1 = Rear
      //7A  3 Air Antenna Select  0 = Front, 1 = Rear
      //7A  4 2 M Antenna Select  0 = Front, 1 = Rear
      //7A  5 UHF Antenna Select  0 = Front, 1 = Rear
      //7A  6 ? ?
      //7A  7 SPL On/Off  0 = Off, 1 = On

      cat[0] = (splitOn ? 0xFF : 0x7F);
      break;
    case 0xB3 : //
      cat[0] = 0x00;
      cat[1] = 0x4D;
      break;

  }

  // sent the data
  Serial.write(cat, 2);
}

void processCATCommand2(byte* cmd) {
  byte response[5];
  unsigned long f;
  int newBand;

  switch (cmd[4]) {
    /*  case 0x00:
        response[0]=0;
        Serial.write(response, 1);
        break;
    */
    case 0x01:
      //set frequency
      f = readFreq(cmd);
      newBand = getBand(f);

      if (newBand != -1) {
        if (newBand != Current_Band) {
          switchBand(newBand, true);
        }

        setFrequency(f);
        updateDisplay();
        response[0] = 0;

      } else {
        response[0] = 0xF0;
      }

      Serial.write(response[0]);
      break;

    case 0x02:
      if ( splitOn == 1 )response[0] = 0xF0;
      else response[0] = 0x00;
      //split on
      splitOn =  1;
      guiUpdate();
      Serial.write(response[0]);
      break;
    case 0x82:
      if ( splitOn == 1 )response[0] = 0x00;
      else response[0] = 0xF0;
      //split off
      splitOn = 0;
      guiUpdate();
      Serial.write(response[0]);
      break;

    case 0x03:
      writeFreq(frequency, response); // Put the frequency into the buffer
      if ( cwMode == 0 ) {
        if (isUSB) response[4] = 0x01; //USB
        else response[4] = 0x00; //LSB
      } else {
        if (isUSB)response[4] = 0x02; //CW
        else response[4] = 0x03; //CWR
      }
      Serial.write(response, 5);
      break;

    case 0x07: // set mode

      switch (cmd[0]) {
        case 0:
          Current_Mode = 1;
          break;
        case 1:
          Current_Mode = 0;
          break;
        case 2:
          Current_Mode = 2;
          break;
        case 3:
          Current_Mode = 3;
          break;
      }

      Set_Mode();
   display_Mode();
  setFrequency(frequency);
   if ( vfoActive == VFO_A )BandModeVFOa[Current_Band] = Current_Mode;
   else BandModeVFOb[Current_Band] = Current_Mode;
     response[0] = 0x00;
      Serial.write(response[0]);

      break;

    case 0x08: // PTT On

      if (!inTx) {
        response[0] = 0;
        Saved_TXON = TXON;
        TXON = true;
        Do_CAT = true;
        if ( cwMode == 0 )startTx(TX_SSB);
        else {
          startTx(TX_CW);
          cwKeydown();
        }

      } else {
        response[0] = 0xf0;
      }
      Serial.write(response[0]);
      break;

    case 0x88 : //PTT OFF
      Do_CAT = false;
      if ( cwMode == 1 )cwKeyUp();
      //stopTx();
      Turn_Off_Carrier_Timer_Count =  Local_Tx_Delay;
      Turn_Off_Carrier_in_Progress = true;
      last_interrupt_time = PTT_HNDKEY_DEBOUNCE_CT;
      PTT_HANDKEY_ACTIVE = false;
      TXON = Saved_TXON;
      response[0] = 0;
      Serial.write(response[0]);
      break;

    case 0x81:
      //toggle the VFOs
      response[0] = 0;
      if (vfoActive == VFO_A)
        switchVFO(VFO_B, true);
      else
        switchVFO(VFO_A, true);
      //menuVfoToggle(1); // '1' forces it to change the VFO
      Serial.write(response[0]);
      updateDisplay();
      break;

    case 0xBB:  //Read FT-817 EEPROM Data  (for comfirtable)
      catReadEEPRom();
      break;

    case 0xe7 :
      //Command E7 - Read Receiver Status:  This command returns one byte.  Its contents are valid only when the '817 is in receive mode and it should be ignored when transmitting.

      //The lower 4 bits (0-3) of this byte indicate the current S-meter reading.
      //00 refers to an S-Zero reading, 04 = S4, 09 = S9, 0A = "10 over," 0B = "20 over" and so on up to 0F.
      //Bit 4  contains no useful information.
      //Bit 5 is 0 in non-FM modes, and it is 0 if the discriminator is centered (within 3.5 kHz for standard FM) when in the FM, FMN, or PKT modes, and 1 if the receiver is off-frequency.
      //Bit 6 is 0 if the CTCSS or DCS is turned off (or in a mode where it is not available.)  It is also 0 if there is a signal being receive and the correct CTCSS tone or DCS code is being decoded.
      //It is 1 if there is a signal and the CTCSS/DCS decoding is enable, but the wrong CTCSS tone, DCS code, or no CTCSS/DCS is present.
      // 0 1 2 3 4 5 6 7 8
      // 0 1 0 0 0 1 0 0 1
      if ( LastSmeter > 49) LastSmeter = 15;
      else if ( LastSmeter > 39) LastSmeter = 14;
      else if ( LastSmeter > 29) LastSmeter = 13;
      else if ( LastSmeter > 19) LastSmeter = 12;
      else if ( LastSmeter > 10) LastSmeter = 11;
      //  Print_Debug("s", LastSmeter);
      response[0] = LastSmeter;
      Serial.write(response[0]);
      break;

    case 0xf7:
      {
        boolean isHighSWR = false;
        boolean isSplitOn = false;

        /*
          Inverted -> *ptt = ((p->tx_status & 0x80) == 0); <-- souce code in ft817.c (hamlib)
        */
        response[0] = ((inTx ? 0 : 1) << 7) +
                      ((isHighSWR ? 1 : 0) << 6) +  //hi swr off / on
                      ((isSplitOn ? 1 : 0) << 5) + //Split on / off
                      (0 << 4) +  //dummy data
                      0x08;  //P0 meter data

        Serial.write(response[0]);
      }
      break;

    default:
      //somehow, get this to print the four bytes
      ultoa(*((unsigned long *)cmd), c, 16);
      /*itoa(cmd[4], b, 16);
        strcat(b, ">");
        strcat(b, c);
        printLine2(b);*/
      response[0] = 0x00;
      Serial.write(response[0]);
  }

  insideCat = false;
}

int catCount = 0;


bool checkCAT( void ) {
  byte i;


  //Check Serial Port Buffer
  if (Serial.available() == 0) {      //Set Buffer Clear status
    rxBufferCheckCount = 0;
    return ( true );
  }

  else if (Serial.available() < 5) {                         //First Arrived
    if (rxBufferCheckCount == 0) {
      rxBufferCheckCount = Serial.available();
      rxBufferArriveTime = millis() + CAT_RECEIVE_TIMEOUT;  //Set time for timeout
    }
    else if (rxBufferArriveTime < millis()) {               //Clear Buffer
      for (i = 0; i < Serial.available(); i++)
        rxBufferCheckCount = Serial.read();
      rxBufferCheckCount = 0;
    }
    else if (rxBufferCheckCount < Serial.available()) {     // Increase buffer count, slow arrive
      rxBufferCheckCount = Serial.available();
      rxBufferArriveTime = millis() + CAT_RECEIVE_TIMEOUT;  //Set time for timeout
    }
    return ( true );
  }



  //Arived CAT DATA
  for (i = 0; i < 13; i++)
    cat[i] = Serial.read();

  if ( cat[0] == 'T' ) {
    cat[11] = 0;
    unsigned long pctime = atol((char*)&cat[1]);
    setTime(pctime);
    local_rtc_set(pctime);
    return ( true );
  } else if ( cat[0] == 'L' ) {
    Serial.write(cat, 1);
    return ( true );
  }
  //this code is not re-entrant.
  if (insideCat == 1)
    return ( true );
  insideCat = 1;

  /**
      This routine is enabled to debug the cat protocol
  **/
  catCount++;

  processCATCommand2(cat);
  insideCat = 0;

  return ( false );

}

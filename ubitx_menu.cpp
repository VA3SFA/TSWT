/** Menus
    File Name ubitx_menu.cpp
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <Adafruit_ILI9341.h>
#include "ubitx.h"
#include "nano_gui.h"
#include <PS2Keyboard.h>
extern PS2Keyboard keyboard;

extern int Freq_Index;
extern unsigned long Freq_Index_Increment[6];

extern Adafruit_ILI9341 tft;

extern unsigned long Band_High_Limits[3][8];
extern unsigned long Band_Low_Limits[3][8];
extern int Region;

extern volatile int TimeToSaveFrequency;

extern void Set_Mode ( void );
extern void   display_Mode( void );
extern int BandModeVFOa[8];
extern int BandModeVFOb[8];

extern void switchBand(int band, bool Display);
extern void setupKeyer();
extern bool Set_Left_Button_Pressed(void);
extern bool Get_Left_Button_Pressed(void);
extern int Get_Mouse_Wheel_Value(void);
extern void Update_Mouse(void);
extern void Save_Mouse(void);
extern void Fake_Delay(void);

extern void displayVFO(int vfo);
extern void LckToggle( void );
extern void Display_RIT(void);
extern void ritToggle(void);
extern void splitToggle(void);
extern int  enc_read(void);
extern void setFrequency(unsigned long f);
extern void updateDisplay(void);

extern void sendKey(char c);
extern void Get_Touch( bool wait );

extern int Frequency_is_Band(unsigned long frequency);
extern int  Current_Mode;

extern volatile boolean Key_Send_Buffer_Full;
extern volatile int     Key_Send_Buffer_IN_Index;
extern volatile int     Key_Send_Buffer_OUT_Index;
extern volatile int     Key_Send_Buffer_Count;
extern int VFOA_BAND;
extern int VFOB_BAND;

extern char Fill_Text[12];
extern char Last_VFO_A_Text[12];
extern char Last_VFO_B_Text[12];



extern unsigned long frequency;
extern char ritOn;

extern char vfoActive;
extern unsigned long vfoA;
extern unsigned long vfoB;
extern unsigned long CW_Timeout;
extern int cwSpeed;
extern char c[30], b[30];
extern unsigned long cwTimeout;
extern unsigned long sideTone;
extern int Menu_Index;                      // new menu
extern unsigned volatile char IAMBIC;  // 0 for Iambic A, 0x10 for Iambic B
extern unsigned volatile char PDLSWAP;     // 0x08 for normal, 0x00 for swap
extern volatile unsigned char keyerControl;
extern bool Volts_Active;
extern bool Tune_Lock;
extern int Current_Band;

extern char Keyboard_Input_Buffer[40];
int KeyIndex = 0;
int OutKeyIndex = 0;
int SendKeyIndex = 0;
int Keyboard_Value;

bool Key_Displayed = false;
bool Creating_Buffered_Text = false;
int Keyboard_Mode = 0;
bool Yellow_Key_Displayed = false;
bool Red_Key_Displayed = false;

//extern int Memory_Number;
bool Memories_Active = false;

extern bool VFO_A_Displayed;
extern bool VFO_B_Displayed;

extern int wpm;
extern int Mem_Number;
int Memory_Table_Select = 0;

int Menu_Cursor_Line_Postions[9] =  {1, 1, 1, 1, 1};
int Menu_Cursor_Index_Postions[9] = {0, 5, 6, 8, 13};
int Max_Menus_Items = 4;
int Current_Menu = 0;
int Last_Menu = 0;
int Last_Mode = 0;

////////////////////////////////////////////////////////////
char Help_Text[12][18] = {
  "Anykey->exit help",
  "c-> Start CW",
  "d-> Band down",
  "f-> Enter Freq",
  "l-> Lock",
  "m-> Mode",
  "r-> RIT",
  "s-> SPLIT",
  "t-> Tone",
  "u-> Band up",
  "v-> VFO Toggle",
  "w-> WPM CW"
};


char Key_buffer0[2];
char Key_buffer1[MAXKYBDENTRY + 2];
char Key_buffer2[MAXKYBDENTRY + 2];
unsigned long Key_Freq;

////////////////////////////////////////////////////////////


void Process_Speed( void ) {
  bool loop = true;
  int Key_Freq;

  Key_Freq = 0;
  SendKeyIndex = 0;
  while ( loop ) {
    while (!keyboard.available()) delay(20);
    Keyboard_Value = keyboard.read();
    // CAR
    if ( Keyboard_Value == 13 ) {
      if ( Key_Freq >= 5 &&  Key_Freq <= 50 ) {
        wpm = Key_Freq;
        cwSpeed = 600 / wpm;
        EEPROM.put(CW_SPEED, cwSpeed);
        Keyboard_Mode = KEY_IDLE;
      }
      tft.fillRect(70, 176, 100, 24, ILI9341_BLUE);
      tft.setCursor(70, 176);
      tft.print(Key_buffer2);
      sprintf(c, "%02d", wpm);
      tft.setTextSize(3);
      displayText(c, 240, 38, 44, 33, DISPLAY_GREEN, DISPLAY_BLUE, DISPLAY_BLUE, false);
      tft.setTextSize(2);
      Keyboard_Mode = KEY_IDLE;
      loop = false;


      Keyboard_Mode = KEY_IDLE;
    } else if ( Keyboard_Value == 0x1b ) {
      itoa(wpm, Key_buffer2, 10);
      tft.fillRect(70, 176, 100, 24, ILI9341_BLUE);
      tft.setCursor(70, 176);
      tft.print(Key_buffer2);
      Keyboard_Mode = KEY_IDLE;
      //  updateDisplay();
    } else if ( (Keyboard_Value == 0x08) || (Keyboard_Value == 0x7F) ) {
      if ( SendKeyIndex > 0 ) {
        SendKeyIndex = SendKeyIndex - 1;
        Key_buffer2[SendKeyIndex] = 0;
        Key_Freq = atol(Key_buffer2);
        tft.fillRect(70, 176, 100, 24, ILI9341_BLUE);
        tft.setCursor(70, 176);
        tft.print(Key_buffer2);
      }
    } else {
      if (isdigit(Keyboard_Value)) {
        if ( SendKeyIndex > 2 )SendKeyIndex = 2;
        else {
          Key_buffer2[SendKeyIndex] =  Keyboard_Value;
          Key_Freq = atol(Key_buffer2);
          tft.fillRect(70, 176, 100, 24, ILI9341_BLUE);
          tft.setCursor(70, 176);
          tft.print(Key_buffer2);
          SendKeyIndex = SendKeyIndex + 1;
        }
      }
    }
  }
}


void Process_Tone( void ) {
  bool loop = true;
  int local_sidetone;

  local_sidetone = 0;
  SendKeyIndex = 0;
  while ( loop ) {
    while (!keyboard.available()) delay(20);
    Keyboard_Value = keyboard.read();
    // CAR
    if ( Keyboard_Value == 13 ) {
      tft.fillRect(2, 176, 300, 24, ILI9341_BLUE);
      if ( local_sidetone > 99 && local_sidetone <= 2000 ) {
        sideTone = local_sidetone;
        EEPROM.put(CW_SIDETONE, sideTone);
      } else {
        tft.fillRect(24, 176, 100, 24, ILI9341_BLUE);
        delay(1000);
      }
      tft.fillRect(2, 176, 300, 24, ILI9341_BLUE);
      Keyboard_Mode = 0;
      SendKeyIndex = 0;
      KeyIndex = 0;
      loop = false;
    } else if ( Keyboard_Value == 0x1b ) {
      tft.fillRect(2, 176, 300, 24, ILI9341_BLUE);
      Keyboard_Mode = 0;
      SendKeyIndex = 0;
      KeyIndex = 0;
      loop = false;
      // ESC
    } else if ( (Keyboard_Value == 127) || (Keyboard_Value == 0x7F) ) {
      if ( SendKeyIndex > 0 ) {
        SendKeyIndex = SendKeyIndex - 1;
        Key_buffer2[SendKeyIndex] = 0;
        local_sidetone = atol(Key_buffer2);
        tft.fillRect(70, 176, 100, 24, ILI9341_BLUE);
        tft.setCursor(70, 176);
        tft.print(Key_buffer2);
      }
    } else {
      if (isdigit(Keyboard_Value)) {
        //sprintf(Key_buffer1, "%c", Keyboard_Value );
        Key_buffer2[SendKeyIndex] = Keyboard_Value;
        SendKeyIndex = SendKeyIndex + 1;
        local_sidetone = atol(Key_buffer2);
        tft.fillRect(70, 176, 100, 24, ILI9341_BLUE);
        tft.setCursor(70, 176);
        tft.print(Key_buffer2);
      }
    }
  } enc_read();
}


void Process_Freq( void ) {
  bool loop = true;

  tft.fillRect(2, 176, 300, 24, ILI9341_BLUE);
  tft.setCursor(2, 176);
  tft.print("f= ");
  strcpy(Key_buffer2, "");

  while ( loop ) {
    while (!keyboard.available()) delay(20);
    Keyboard_Value = keyboard.read();

    if ( Keyboard_Value == 13 ) {
      tft.fillRect(2, 176, 200, 24, ILI9341_BLUE);
      Serial.println(Key_Freq);
      if ( ( Key_Freq <= Band_High_Limits[Region - 1][Current_Band]) &&
           ( Key_Freq >= Band_Low_Limits[Region - 1][Current_Band])) {
        frequency = Key_Freq;
        setFrequency(frequency);
        updateDisplay();
      } else {
        tft.fillRect(24, 176, 100, 24, ILI9341_BLUE);
        delay(1000);
      }
      tft.fillRect(2, 176, 300, 24, ILI9341_BLUE);
      Keyboard_Mode = 0;
      SendKeyIndex = 0;
      KeyIndex = 0;
      loop = false;
    } else if ( Keyboard_Value == 0x1b ) {
      tft.fillRect(2, 176, 300, 24, ILI9341_BLUE);
      Keyboard_Mode = 0;
      SendKeyIndex = 0;
      KeyIndex = 0;
      loop = false;
    } else if ( (Keyboard_Value == 127) || (Keyboard_Value == 0x7F) ) {
      if ( SendKeyIndex > 0 ) {
        SendKeyIndex = SendKeyIndex - 1;
        Key_buffer2[strlen(Key_buffer2) - 1] = 0;
        Key_Freq = atol(Key_buffer2);
      }
    } else {
      if (isdigit(Keyboard_Value)) {
        SendKeyIndex = SendKeyIndex + 1;
        sprintf(Key_buffer1, "%c", Keyboard_Value );
        strcat(Key_buffer2, Key_buffer1);
        Key_Freq = atol(Key_buffer2);
        tft.fillRect(70, 176, 100, 24, ILI9341_BLUE);
        tft.setCursor(70, 176);
        tft.print(Key_buffer2);
      }
    }
  } enc_read();
}

void Process_Idle( void ) {
  int i;
  bool loop;


  Keyboard_Mode = Keyboard_Input_Buffer[OutKeyIndex];

  switch ( Keyboard_Input_Buffer[OutKeyIndex] ) {

    case KEY_CW:
      if ( (Current_Mode == 2) || (Current_Mode == 3) || (Current_Mode == 4)) {
        Keyboard_Mode = KEY_CW;
        strcpy(Key_buffer2, ">");
        //  tftPrint(2,
        tft.fillRect(2, 176, 316, 24, ILI9341_BLACK);
        strcpy(Key_buffer1, "");
        strcpy(Key_buffer2, "");
        Key_Displayed = true;
        Yellow_Key_Displayed = false;
        Red_Key_Displayed = false;
        Key_Freq = 0;
      }
      break;

    case KEY_BAND_DOWN:
      tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
      tft.setCursor(2, 176);
      tft.println("Band DOWN");
      loop = true;
      while ( loop ) {
        if ( 'd' == Keyboard_Input_Buffer[OutKeyIndex]) {
          Current_Band = Current_Band - 1;
          if ( Current_Band < 0 )Current_Band = 7;
          switchBand(Current_Band, true);
          while (!keyboard.available()) delay(20);
          Keyboard_Input_Buffer[OutKeyIndex] = keyboard.read();
        } else loop = false;
      }
      tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
      break;

    case KEY_FREQ:
      strcpy(Key_buffer1, "");
      strcpy(Key_buffer2, "");
      Key_Displayed = true;
      Key_Freq = 0;
      Process_Freq();
      break;

    case KEY_MODE:
      tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
      tft.setCursor(2, 176);
      tft.println("Mode");
      loop = true;
      while ( loop ) {
        if ( 'm' == Keyboard_Input_Buffer[OutKeyIndex]) {
          Current_Mode++;
          if ( Current_Mode > 4) Current_Mode = 0;
          Set_Mode();
          display_Mode();
          setFrequency(frequency);
          if ( vfoActive == VFO_A )BandModeVFOa[Current_Band] = Current_Mode;
          else BandModeVFOb[Current_Band] = Current_Mode;
          TimeToSaveFrequency = TimeUntilSave_Frequency;
          while (!keyboard.available()) delay(20);
          Keyboard_Input_Buffer[OutKeyIndex] = keyboard.read();
        } else loop = false;
      }
      tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
      break;


    case KEY_LOCK:
      LckToggle();
      displayVFO(VFO_A);
      displayVFO(VFO_B);
      break;

    case KEY_VFO:
      if ( !ritOn ) {
        if (vfoActive == VFO_B)
          switchVFO(VFO_A, true);
        else
          switchVFO(VFO_B, true);
      }
      break;

    case KEY_RIT:
      ritToggle();
      break;

    case KEY_SPLIT:
      splitToggle();
      break;

    case KEY_WPM:
      Keyboard_Mode = KEY_WPM;
      for ( i = 0; i < signed(sizeof(Key_buffer2)); i++) Key_buffer2[i] = 0;
      tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
      tft.setCursor(2, 176);
      tft.println("w= ");
      Process_Speed();
      break;

    case KEY_TONE:
      Keyboard_Mode = KEY_TONE;
      for ( i = 0; i < signed(sizeof(Key_buffer2)); i++) Key_buffer2[i] = 0;
      Key_Displayed = true;
      tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
      tft.setCursor(2, 176);
      tft.println("T= ");
      Process_Tone();
      break;

    case KEY_BAND_UP:
      tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
      tft.setCursor(2, 176);
      tft.println("Band UP");
      loop = true;
      while ( loop ) {
        if ( 'u' == Keyboard_Input_Buffer[OutKeyIndex]) {
          Current_Band = Current_Band + 1;
          if ( Current_Band > 7 )Current_Band = 0;
          switchBand(Current_Band, true);
          while (!keyboard.available()) delay(20);
          Keyboard_Input_Buffer[OutKeyIndex] = keyboard.read();
        } else loop = false;
      }
      tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
      break;

    case KEY_HELP:
      i = 0;
      while (!keyboard.available()) {
        tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
        tft.setCursor(2, 176);
        tft.println(Help_Text[i]);
        if ( !keyboard.available())delay(1000);
        if ( !keyboard.available())delay(1000);
        i = i + 1;
        if ( i >= 12 ) i = 0;
      }
      keyboard.read();
      break;

    default:
      break;
  }
  KeyIndex = 0;
  SendKeyIndex = 0;
}

int Local_Key_Send_Buffer_Count;


void Process_CW( void ) {
  int i;
  KeyIndex = 0;

  noInterrupts();
  Local_Key_Send_Buffer_Count = Key_Send_Buffer_Count;
  interrupts();

  Key_Displayed = false;
  Keyboard_Value = Keyboard_Input_Buffer[0];

  if ( Keyboard_Value == 0x1b ) {
    while ( Key_Send_Buffer_Count > 0 ) {
      delay(100);
    }
    noInterrupts();
    Key_Send_Buffer_Count = 0;
    Key_Send_Buffer_IN_Index = 0;
    Key_Send_Buffer_OUT_Index = 0;
    sendKey(toupper(0));
    interrupts();
    Yellow_Key_Displayed = false;
    Red_Key_Displayed = false;
    tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
    Keyboard_Mode = KEY_IDLE;

  } else if ( Keyboard_Value == 0x0A ) {
    sendKey(toupper(0));
    tft.fillRect(2, 176, 316, 24, ILI9341_BLACK);
    Keyboard_Mode = KEY_IDLE;

  } else {
    if ( Local_Key_Send_Buffer_Count > (MAX_KEYING_BUFFER - 25)) {
      if (Yellow_Key_Displayed == false )Yellow_Key_Displayed = true;
      tft.fillRect(2, 176, 316, 24, ILI9341_YELLOW);
      tft.setTextColor(ILI9341_RED, ILI9341_WHITE);
      tft.setCursor(8, 176);
      tft.print(Key_buffer1);
    } else {
      SendKeyIndex = SendKeyIndex + 1;
      sprintf(Key_buffer0, "%c", toupper(Keyboard_Value));
      if ( SendKeyIndex > MAXKYBDENTRY  ) {
        for (i = 0; i < MAXKYBDENTRY - 1; i++)Key_buffer1[i] = Key_buffer1[i + 1];
        SendKeyIndex = MAXKYBDENTRY;
        Key_buffer1[MAXKYBDENTRY - 1] = Key_buffer0[0];
        Key_buffer1[MAXKYBDENTRY] = 0;
      } else strcat(Key_buffer1, Key_buffer0);
      tft.fillRect(2, 176, 316, 24, ILI9341_BLACK);
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
      tft.setCursor(8, 176);
      tft.print(Key_buffer1);
      if (Keyboard_Value == ' ')Keyboard_Value = 1;
      sendKey(toupper(Keyboard_Value));
    }
  }
}




void Process_Keyboard( void ) {

  if (keyboard.available()) {
    Keyboard_Input_Buffer[0] = keyboard.read();

    if ( Keyboard_Mode == KEY_CW )Process_CW();
    else {
      if ( ( KeyIndex == 0 ) && (Keyboard_Input_Buffer[0] == 27 )) {
        OutKeyIndex = 0;
        Keyboard_Mode = Keyboard_Input_Buffer[0];
        tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
        tft.setCursor(2, 176);
        tft.println("Kcmd> (h-help)");
        Keyboard_Mode = KEY_CMD;
        KeyIndex = 1;
      } else if ( Keyboard_Mode == KEY_CMD) {
        Process_Idle();
        if ( Keyboard_Mode != KEY_CW) {
          Keyboard_Mode = KEY_IDLE;
          tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
        }
      } else {
        KeyIndex = 0;
        tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
        tft.setCursor(2, 176);
        tft.println("Esc -> keyboard");
        delay(2000);
        tft.fillRect(2, 176, 316, 24, ILI9341_BLUE);
        enc_read();
      }
    } enc_read();
  }
}

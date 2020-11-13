/**
   File name ubitx_keyer.cpp
   CW Keyer

   The CW keyer handles either a straight key or an iambic / paddle key.
   D12 for DOT Paddle  and D11 for DASH Paddle  and D* for PTT/Handkey

   Generating CW
   The CW is cleanly generated by unbalancing the front-end mixer
   and putting the local oscillator directly at the CW transmit frequency.
   The sidetone, generated by the Arduino is injected into the volume control
*/
#include <Arduino.h>
#include "ubitx.h"

#include <Adafruit_ILI9341.h>
extern Adafruit_ILI9341 tft;

volatile bool Do_CAT = false;
/////////////////////////  keyer

#define DIT_L 0x01 // DIT latch
#define DAH_L 0x02 // DAH latch
#define DIT_PROC 0x04 // DIT is being processed
enum KSTYPE {IDLE, CHK_DIT, CHK_DAH, KEYED_PREP, KEYED, INTER_ELEMENT };


extern void startTx(byte txMode);
extern void updateDisplay(void);
extern volatile  byte rxBufferCheckCount;
extern bool checkCAT();
extern unsigned long sideTone;
extern volatile int ubitx_mode;

extern volatile unsigned char keyerControl;
volatile unsigned char keyerState;
extern unsigned volatile char PDLSWAP;

extern int wpm;
volatile bool TXON = true;
volatile int cwMode;

volatile bool TX_Inhibit = false;


volatile uint8_t Last_Bits = 0xFF;;

volatile int TimeToSaveFrequency = -1;

volatile bool Dot_in_Progress = false;
volatile unsigned long  Dot_Timer_Count = 0;
volatile bool Dash_in_Progress = false;
volatile unsigned long  Dash_Timer_Count = 0;
volatile bool Inter_Bit_in_Progress = false;
volatile unsigned long  Inter_Bit_Timer_Count = 0;
volatile bool Turn_Off_Carrier_in_Progress = false;
volatile unsigned long  Turn_Off_Carrier_Timer_Count = 0;
volatile bool PTT_HANDKEY_ACTIVE = false;
volatile bool MANUAL_KEY_ACTIVE = false;
volatile long last_interrupt_time = 20;
extern bool Cat_Lock;
volatile bool TX_In_Progress;
volatile int smeterInterval;


volatile boolean Key_Send_Buffer_Full;
volatile unsigned char    Key_Send_Buffer[MAX_KEYING_BUFFER + 2];
volatile int     Key_Send_Buffer_IN_Index;
volatile int     Key_Send_Buffer_OUT_Index;
volatile int     Key_Send_Buffer_Count;
volatile int     Key_Send_Buffer_Bit_Count = 0;


volatile unsigned char pp;


#define KEY_SEND_STATE_START_BYTE                       0
#define KEY_SEND_STATE_WAIT_FOR_BYTE_TO_COMPLETE       1
#define KEY_SEND_STATE_DO_BYTE_BIT                      2
#define KEY_SEND_STATE_COMPLETE_BYTE                    3
#define KEY_SEND_STATE_PROCESS_BYTE                     4
#define KEY_SEND_STATE_PROCESS_BIT                     5
#define KEY_SEND_STATE_WAIT_FOR_BIT_TO_COMPLETE       6

volatile int     Key_Send_Buffer_State = KEY_SEND_STATE_START_BYTE;


IntervalTimer KEYER_TIMER;

int ledState = LOW;
unsigned long Local_Tx_Delay;

/**
   Starts transmitting the carrier with the sidetone
   It assumes that we have called cwTxStart and not called cwTxStop
   each time it is called, the cwTimeOut is pushed further into the future
*/
void cwKeydown(void) {
  if ( TXON ) digitalWrite(CW_KEY, 1);
  keyDown = 1;                  //tracks the CW_KEY
  tone(CW_TONE, (int)sideTone);

}
/**
   Stops the CW carrier transmission along with the sidetone
   Pushes the cwTimeout further into the future
*/
void cwKeyUp(void) {
  keyDown = 0;    //tracks the CW_KEY
  noTone(CW_TONE);
  if ( TXON )   digitalWrite(CW_KEY, 0);

}


// code provided by W3IVD
// enum CW_PORT  { KEYBOARD, PADDLE_NORM, PADDLE_REVERSE, MANUAL, COOTIE };

void update_PaddleLatch() {


  if ( CW_Port_Type == PADDLE_NORM ) {  //process reverse paddle
    if (digitalRead(DIGITAL_DOT) == LOW) {
      keyerControl |= DIT_L;
    }
    if (digitalRead(DIGITAL_DASH) == LOW) {
      keyerControl |= DAH_L;
    }
  } else {
    if (digitalRead(DIGITAL_DOT) == LOW) {
      keyerControl |= DAH_L;
    }
    if (digitalRead(DIGITAL_DASH) == LOW) {
      keyerControl |= DIT_L;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////
//   interupt  handlers

////   timers
void Keyer_Timer(void) {
  bool continue_loop = true;


  // check to save freq to EEPROM
  if ( TimeToSaveFrequency > 0 ) {
    TimeToSaveFrequency = TimeToSaveFrequency - 1;  // when reaches zero "loop" will save it
  }

  // process display smeter
  if (  smeterInterval > 0 )  smeterInterval = smeterInterval - 1;


  if ( !TX_Inhibit ) {
    if ( !Do_CAT ) {


      Local_Tx_Delay  = cwDelayTime;

      // process if CW modes
      if ( ((Key_Send_Buffer_Count > 0) | (CW_Port_Type != KEYBOARD)) && (cwMode == 1) ) {


        // process DOT and DASH timing
        if ( (Dot_in_Progress) && (Dot_Timer_Count > 0) ) {
          if ( !inTx ) {
            keyDown = 0;
            startTx(TX_CW);
          }
          if ( keyDown == 0 ) cwKeydown();
          Dot_Timer_Count = Dot_Timer_Count - 1;
          if ( Dot_Timer_Count <= 0 ) {
            Dot_Timer_Count = 0;
            Dot_in_Progress = false;
            cwKeyUp();
          }
        }

        // process Inter Bit Timing
        if ( (Inter_Bit_in_Progress) && (Inter_Bit_Timer_Count > 0) ) {
          Inter_Bit_Timer_Count = Inter_Bit_Timer_Count - 1;
          if ( Inter_Bit_Timer_Count <= 0 ) {
            Inter_Bit_Timer_Count = 0;
            Inter_Bit_in_Progress = false;
          }
        }

        // process turning off carrier
        if ( (Turn_Off_Carrier_in_Progress) && (Turn_Off_Carrier_Timer_Count > 0) ) {
          Turn_Off_Carrier_Timer_Count = Turn_Off_Carrier_Timer_Count - 1;
          if ( Turn_Off_Carrier_Timer_Count <= 0 ) {
            Turn_Off_Carrier_in_Progress = false;
            Turn_Off_Carrier_Timer_Count = 0;
            if ( inTx )stopTx();
          }
        }

        // process hand key or manual key in TSW key jack
        if ( (digitalRead(HAND_KEY) == 0)  ||
             ((CW_Port_Type == MANUAL) && (digitalRead(DIGITAL_DOT) == 0)) ||
             ((CW_Port_Type == COOTIE) && ( (digitalRead(DIGITAL_DOT) == 0) || (digitalRead(DIGITAL_DASH) == 0) ))
           ) {
          // If interrupts come faster than 5ms, assume it's a bounce and ignore
          last_interrupt_time = last_interrupt_time - 1;
          if ( last_interrupt_time  <= 0) {
            last_interrupt_time = 0;
            if ( !inTx ) {
              keyDown = 0;
              startTx(TX_CW);
            }
            if ( keyDown == 0 ) cwKeydown();
            PTT_HANDKEY_ACTIVE = true;
            Turn_Off_Carrier_Timer_Count =  Local_Tx_Delay;
          }
        } else if ( (keyDown == 1) && (PTT_HANDKEY_ACTIVE == true) ) {
          cwKeyUp();
          Turn_Off_Carrier_Timer_Count =  Local_Tx_Delay;
          Turn_Off_Carrier_in_Progress = true;
          last_interrupt_time = PTT_HNDKEY_DEBOUNCE_CT;
          PTT_HANDKEY_ACTIVE = false;
        } else  last_interrupt_time = PTT_HNDKEY_DEBOUNCE_CT;

        if (( PTT_HANDKEY_ACTIVE == false ) &&
            (( CW_Port_Type != MANUAL ) && ( CW_Port_Type != COOTIE ))
           ) {

          while (continue_loop) {
            switch (keyerState) {
              case IDLE:
                if (
                  (!digitalRead(DIGITAL_DOT)) ||
                  (!digitalRead(DIGITAL_DASH)) ||
                  (keyerControl & 0x03)
                ) {
                  update_PaddleLatch();
                  keyerState = CHK_DIT;
                  Dot_in_Progress = false;
                  Dot_Timer_Count = 0;
                  Turn_Off_Carrier_Timer_Count = 0;
                  Turn_Off_Carrier_in_Progress = false;
                } else {
                  continue_loop = false;
                }
                break;

              case CHK_DIT:
                if (keyerControl & DIT_L) {
                  keyerControl |= DIT_PROC;
                  keyerState = KEYED_PREP;
                  Dot_Timer_Count = cwSpeed;
                } else {
                  keyerState = CHK_DAH;
                }
                break;

              case CHK_DAH:
                if (keyerControl & DAH_L) {
                  keyerState = KEYED_PREP;
                  Dot_Timer_Count = cwSpeed * 3;
                } else {
                  continue_loop = false;
                  keyerState = IDLE;
                }
                break;

              case KEYED_PREP:
                keyerControl &= ~(DIT_L + DAH_L); // clear both paddle latch bits
                keyerState = KEYED; // next state
                Turn_Off_Carrier_Timer_Count = 0;
                Turn_Off_Carrier_in_Progress = false;
                Dot_in_Progress = true;
                break;

              case KEYED:
                if (Dot_in_Progress == false) { // are we at end of key down ?
                  Inter_Bit_in_Progress = true;
                  Inter_Bit_Timer_Count = cwSpeed;
                  keyerState = INTER_ELEMENT; // next state
                  //       } else if (keyerControl & IAMBIC) {
                } else {
                  update_PaddleLatch(); // early paddle latch in Iambic B mode
                  continue_loop = false;
                }
                //   } else continue_loop = false;
                break;

              case INTER_ELEMENT:
                // Insert time between dits/dahs
                update_PaddleLatch(); // latch paddle state
                if (Inter_Bit_in_Progress == false) { // are we at end of inter-space ?
                  Turn_Off_Carrier_Timer_Count = Local_Tx_Delay;
                  Turn_Off_Carrier_in_Progress = true;
                  if (keyerControl & DIT_PROC) { // was it a dit or dah ?
                    keyerControl &= ~(DIT_L + DIT_PROC); // clear two bits
                    keyerState = CHK_DAH; // dit done, check for dah
                  } else {
                    keyerControl &= ~(DAH_L); // clear dah latch
                    keyerState = IDLE; // go idle
                  }
                } else continue_loop = false;
                break;
            }
          }

        }
        // process PTT
      } else  {
        if (  digitalRead(PTT) == 0 ) {
          // If interrupts come faster than 5ms, assume it's a bounce and ignore
          last_interrupt_time = last_interrupt_time - 1;
          if ( last_interrupt_time <= 0) {
            last_interrupt_time = 0;
            if ( !inTx ) startTx(TX_SSB);
          }
        } else if ( (inTx) && (TX_In_Progress == false )) {
          last_interrupt_time = PTT_HNDKEY_DEBOUNCE_CT;
          stopTx();
        } else last_interrupt_time = PTT_HNDKEY_DEBOUNCE_CT;
      }
      /////////////////////////////////  Interrupt Processing of Keyboard Data  ////////////////////////

      if ((CW_Port_Type == KEYBOARD) && ( Key_Send_Buffer_Count > 0) ) {

        switch (Key_Send_Buffer_State) {

          case KEY_SEND_STATE_START_BYTE:

            if (Key_Send_Buffer[Key_Send_Buffer_OUT_Index] == 1) {
              Inter_Bit_Timer_Count = 7 * cwSpeed ;
              Inter_Bit_in_Progress = true;
              Key_Send_Buffer_State = KEY_SEND_STATE_WAIT_FOR_BYTE_TO_COMPLETE;
            } else if ( Key_Send_Buffer[Key_Send_Buffer_OUT_Index] == 0) {
              Turn_Off_Carrier_Timer_Count = Local_Tx_Delay;
              Turn_Off_Carrier_in_Progress = true;
              Key_Send_Buffer_State = KEY_SEND_STATE_COMPLETE_BYTE;
            } else {
              Key_Send_Buffer_Bit_Count = 0;
              pp = Key_Send_Buffer[Key_Send_Buffer_OUT_Index];
              Key_Send_Buffer_State = KEY_SEND_STATE_PROCESS_BYTE;
            }
            break;

          case KEY_SEND_STATE_PROCESS_BYTE:
            if (pp != 1) {
              if (pp & 1) Dot_Timer_Count = cwSpeed * 3;
              else Dot_Timer_Count = cwSpeed;
              Dot_in_Progress = true;
              pp = pp / 2 ;
              Key_Send_Buffer_State = KEY_SEND_STATE_PROCESS_BIT;
            } else {
              Inter_Bit_Timer_Count = 2 * cwSpeed ;
              Inter_Bit_in_Progress = true;
              Key_Send_Buffer_State = KEY_SEND_STATE_WAIT_FOR_BYTE_TO_COMPLETE;
            }
            break;

          case KEY_SEND_STATE_PROCESS_BIT:
            if ( Dot_in_Progress == false ) {
              Inter_Bit_Timer_Count = cwSpeed ;
              Inter_Bit_in_Progress = true;
              Key_Send_Buffer_State = KEY_SEND_STATE_WAIT_FOR_BIT_TO_COMPLETE;
            }
            break;

          case KEY_SEND_STATE_WAIT_FOR_BIT_TO_COMPLETE:
            if ( Inter_Bit_in_Progress == false ) Key_Send_Buffer_State = KEY_SEND_STATE_PROCESS_BYTE;
            break;

          case KEY_SEND_STATE_WAIT_FOR_BYTE_TO_COMPLETE:
            if ( Inter_Bit_in_Progress == false ) {
              Key_Send_Buffer_Count = Key_Send_Buffer_Count - 1;
              Key_Send_Buffer_OUT_Index++;
              Key_Send_Buffer_State = KEY_SEND_STATE_START_BYTE;
              Turn_Off_Carrier_Timer_Count = 5 * Local_Tx_Delay;
              Turn_Off_Carrier_in_Progress = true;
            }
            break;

          case KEY_SEND_STATE_COMPLETE_BYTE:
            Key_Send_Buffer_Count = Key_Send_Buffer_Count - 1;
            if (Key_Send_Buffer_Count < 0 )Key_Send_Buffer_Count = 0;
            Key_Send_Buffer_OUT_Index++;
            Key_Send_Buffer_State = KEY_SEND_STATE_START_BYTE;
            break;

        }

      }

    }

  }

}



/////////////////////////////////  Interrupt Processing of Keyboard Data  ////////////////////////

void Connect_Interrupts(void) {
  // Create an keyer timer object
  KEYER_TIMER.begin( Keyer_Timer, 2000 );
}



#define     N_MORSE  (sizeof(morsetab)/sizeof(morsetab[0]))
// Morse table
struct t_mtab {
  char c, pat;
} ;
struct t_mtab morsetab[] = {
  {'.', 106}, {',', 115}, {'?', 76},  {'/', 41}, {'A', 6},  {'B', 17}, {'C', 21}, {'D', 9},
  {'E', 2},   {'F', 20},  {'G', 11}, {'H', 16}, {'I', 4},  {'J', 30}, {'K', 13}, {'L', 18},
  {'M', 7},   {'N', 5},   {'O', 15}, {'P', 22}, {'Q', 27}, {'R', 10}, {'S', 8},  {'T', 3},
  {'U', 12},  {'V', 24},  {'W', 14}, {'X', 25}, {'Y', 29}, {'Z', 19}, {'1', 62}, {'2', 60},
  {'3', 56},  {'4', 48},  {'5', 32}, {'6', 33}, {'7', 35}, {'8', 39}, {'9', 47}, {'0', 63},
  {'=', 49},  {'<', 42},  {'>', 104}, {';', 85}, {'@', 86}, {'!', 117}, {'(', 45}, {'$', 200},
  //{')',109},  {'"', 82},  {'-', 97}, {'_',108}, {':', 71}, {'+', 42}, {39,94}
  {')', 109},  {'"', 82},  {'-', 97}, {'_', 108}, {':', 71}, {'+', 42}, {'\'', 94}
};
///////////////////////////////////////////////////////////////////////////////////////////
// prosign mapping
/*

  = is mapped to BT
  < is mapped to AR
  > is mapped to SK
  [ is mapped to QRZ
  ] is mapped to QSL
  / is mapped to DN
  ; is mapped to KR
  @ is mapped to AC
  ! is mapped to CM
  ( is mapped to KN
  $ is mapped to VU
  ) is mapped to KK
  “ is mapped to RR
  - is mapped to DU
  ‘ is mapped to WG
  _ is mapped to UK
  : is mapped to OS
  + is mapped to AR

*/
///////////////////////////////////////////////////////////////////////////////////////////



// CW generation routines for CQ message
void key(int LENGTH) {

  if ( !inTx ) startTx(TX_CW);
  cwKeydown();
  delay(LENGTH * 2);
  cwKeyUp();
  delay(cwSpeed * 2);

}


void send(char c) {
  int i ;

  if (c == ' ') {
    delay(7 * cwSpeed) ;
    return ;
  }
  for (i = 0; i < signed(N_MORSE); i++) {
    if (morsetab[i].c == c) {
      unsigned char p = morsetab[i].pat ;
      while (p != 1) {
        if (p & 1) Dot_Timer_Count = cwSpeed * 3;
        else Dot_Timer_Count = cwSpeed;
        key(Dot_Timer_Count);
        p = p / 2 ;
      }
      delay(cwSpeed * 5) ;
      return ;
    }
  }
}


void sendKey(char c) {
  int i;


  if (c == ' ') {
    if ( Key_Send_Buffer_Count < MAX_KEYING_BUFFER ) {
      Key_Send_Buffer[Key_Send_Buffer_IN_Index++] = c;
      Key_Send_Buffer_Count++;
      Key_Send_Buffer_Full = false;
    } else  Key_Send_Buffer_Full = true;
  } else {
    for (i = 0; i < signed(N_MORSE); i++) {
      if (morsetab[i].c == c) {
        if ( Key_Send_Buffer_Count < MAX_KEYING_BUFFER ) {
          Key_Send_Buffer[Key_Send_Buffer_IN_Index++] = morsetab[i].pat;
          Key_Send_Buffer_Count++;
          Key_Send_Buffer_Full = false;
        } else  Key_Send_Buffer_Full = true;
      }
    }
  }
}



void sendmsg(char *str) {

  while (*str) send(*str++);
  delay(650);
  stopTx();
}
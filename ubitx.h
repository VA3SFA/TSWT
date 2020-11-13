
//   To activate PS2 Keyboard uncomment the line below
//  #define KEYBOARD_INCLUDED


struct Point {
  int x, y;
};

const int DataPin = A8;
const int IRQpin =  A9;



/* The ubitx is powered by an arduino nano. The pin assignment is as folows 
 *  
 */

#define ENC_A (A0)          // Tuning encoder interface
#define ENC_B (A1)          // Tuning encoder interface
#define FBUTTON (A2)        // Tuning encoder interface
#define PTT   (A3)          // Sense it for ssb and as a straight key for cw operation
#define SMETERREADPIN (A7)
#define DIGITAL_DOT  (A8)
#define DIGITAL_DASH (A9)
#define HAND_KEY (A3)

#define TX_RX (7)           // Pin from the Nano to the radio to switch to TX (HIGH) and RX(LOW)
#define CW_TONE (6)         // Generates a square wave sidetone while sending the CW. 
#define TX_LPF_A (5)        // The 30 MHz LPF is permanently connected in the output of the PA... 
#define TX_LPF_B (4)        //  ...Alternatively, either 3.5 MHz, 7 MHz or 14 Mhz LPFs are...
#define TX_LPF_C (3)        //  ...switched inline depending upon the TX frequency
#define CW_KEY (2)          //  Pin goes high during CW keydown to transmit the carrier. 
                            // ... The CW_KEY is needed in addition to the TX/RX key as the...
                            // ...key can be up within a tx period

#define USB 0      usb=1  cw=0
#define LSB 1      usb=0  cw=0
#define CW  2      usb=1  cw=1
#define CWR 3      usb=0  cw=1
#define CWP 4      usb=1  cw=1

#define PTT_HNDKEY_DEBOUNCE_CT 2

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
#define CS_PIN  8     //this is the pin to select the touch controller on spi interface
// MOSI=11, MISO=12, SCK=13

//XPT2046_Touchscreen ts(CS_PIN);

//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

/**
 * The Arduino, unlike C/C++ on a regular computer with gigabytes of RAM, has very little memory.
 * We have to be very careful with variables that are declared inside the functions as they are 
 * created in a memory region called the stack. The stack has just a few bytes of space on the Arduino
 * if you declare large strings inside functions, they can easily exceed the capacity of the stack
 * and mess up your programs. 
 * We circumvent this by declaring a few global buffers as  kitchen counters where we can 
 * slice and dice our strings. These strings are mostly used to control the display or handle
 * the input and output from the USB port. We must keep a count of the bytes used while reading
 * the serial port as we can easily run out of buffer space. This is done in the serial_in_count variable.
 */
extern char c[30], b[30];      
extern char printBuff[2][20];  //mirrors what is showing on the two lines of the display
extern int count;          //to generally count ticks, loops, etc

/** 
 *  The second set of 16 pins on the Raduino's bottom connector are have the three clock outputs and the digital lines to control the rig.
 *  This assignment is as follows :
 *    Pin   1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16
 *         GND +5V CLK0  GND  GND  CLK1 GND  GND  CLK2  GND  D2   D3   D4   D5   D6   D7  
 *  These too are flexible with what you may do with them, for the Raduino, we use them to :
 *  - TX_RX line : Switches between Transmit and Receive after sensing the PTT or the morse keyer
 *  - CW_KEY line : turns on the carrier for CW
 */


/**
 * These are the indices where these user changable settinngs are stored  in the EEPROM
 */
#define MASTER_CAL 0
#define LSB_CAL 4
#define USB_CAL 8
#define SIDE_TONE 12
//these are ids of the vfos as well as their offset into the eeprom storage, don't change these 'magic' values
#define VFO_A 16
#define VFO_B 20
#define CW_SIDETONE 24
#define CW_SPEED 28
// the screen calibration parameters : int slope_x=104, slope_y=137, offset_x=28, offset_y=29;
#define SLOPE_X 32
#define SLOPE_Y 36
#define OFFSET_X 40
#define OFFSET_Y 44
#define CW_DELAYTIME 48
// 
#define BANDFREQVFOa_80  52
#define BANDFREQVFOa_40  56
#define BANDFREQVFOa_30  60
#define BANDFREQVFOa_20  64
#define BANDFREQVFOa_17  68
#define BANDFREQVFOa_15  72
#define BANDFREQVFOa_10  76
#define BANDFREQVFOa_GEN 80
// 
#define BANDFREQVFOb_80  84
#define BANDFREQVFOb_40  88
#define BANDFREQVFOb_30  92
#define BANDFREQVFOb_20  96
#define BANDFREQVFOb_17  100
#define BANDFREQVFOb_15  104
#define BANDFREQVFOb_10  108
#define BANDFREQVFOb_GEN 112

#define LASTBANDVFOA     116
#define LASTBANDVFOB     120

#define SMTRACTIVE       124
#define SMTRCONSTANT     128

#define MEMBANK01_0      132
#define MEMBANK01_1      136
#define MEMBANK01_2      140
#define MEMBANK01_3      144
#define MEMBANK01_4      148
#define MEMBANK01_5      152
#define MEMBANK01_6      156
#define MEMBANK01_7      160
#define MEMBANK01_8      164
#define MEMBANK01_9      168

#define RTC_CLOCK        172
// 
#define BANDMODEVFOa_80  176
#define BANDMODEVFOa_40  180
#define BANDMODEVFOa_30  184
#define BANDMODEVFOa_20  188
#define BANDMODEVFOa_17  192
#define BANDMODEVFOa_15  196
#define BANDMODEVFOa_10  200
#define BANDMODEVFOa_GEN 204
// 
#define BANDMODEVFOb_80  208
#define BANDMODEVFOb_40  212
#define BANDMODEVFOb_30  216
#define BANDMODEVFOb_20  220
#define BANDMODEVFOb_17  224
#define BANDMODEVFOb_15  228
#define BANDMODEVFOb_10  232
#define BANDMODEVFOb_GEN 236

#define SELECTED_VFO     240
#define SELECTED_FREQ_INDEX 244
#define REGION           248
////////////////////////////// end EEPROM Definitions
#define TimeUntilSave_Frequency 2000    //seconds divided by 20 msec timer


//These are defines for the new features back-ported from KD8CEC's software
//these start from beyond 256 as Ian, KD8CEC has kept the first 256 bytes free for the base version


// handkey, iambic a, iambic b : 0,1,2f
#define CW_KEY_TYPE 358

/**
 * The uBITX is an upconnversion transceiver. The first IF is at 45 MHz.
 * The first IF frequency is not exactly at 45 Mhz but about 5 khz lower,
 * this shift is due to the loading on the 45 Mhz crystal filter by the matching
 * L-network used on it's either sides.
 * The first oscillator works between 48 Mhz and 75 MHz. The signal is subtracted
 * from the first oscillator to arriive at 45 Mhz IF. Thus, it is inverted : LSB becomes USB
 * and USB becomes LSB.
 * The second IF of 11.059 Mhz has a ladder crystal filter. If a second oscillator is used at 
 * 56 Mhz (appox), the signal is subtracted FROM the oscillator, inverting a second time, and arrives 
 * at the 11.059 Mhz ladder filter thus doouble inversion, keeps the sidebands as they originally were.
 * If the second oscillator is at 33 Mhz, the oscilaltor is subtracated from the signal, 
 * thus keeping the signal's sidebands inverted. The USB will become LSB.
 * We use this technique to switch sidebands. This is to avoid placing the lsbCarrier close to
 * 11 MHz where its fifth harmonic beats with the arduino's 16 Mhz oscillator's fourth harmonic
 */

#define INIT_USB_FREQ   (11059200l)
// limits the tuning and working range of the ubitx between 3 MHz and 30 MHz
#define LOWEST_FREQ   (100000l)
#define HIGHEST_FREQ (30000000l)

//we directly generate the CW by programmin the Si5351 to the cw tx frequency, hence, both are different modes
//these are the parameter passed to startTx
#define TX_SSB 0
#define TX_CW 1

extern char ritOn;
extern char vfoActive;
extern unsigned long vfoA, vfoB, sideTone, usbCarrier;
extern char isUsbVfoA, isUsbVfoB;
extern unsigned long frequency, ritRxFrequency, ritTxFrequency;  //frequency is the current frequency on the dial
extern unsigned long firstIF;



//these are variables that control the keyer behaviour
extern int cwSpeed; //this is actuall the dot period in milliseconds
extern int wpm;
extern int32_t calibration;
extern int cwDelayTime;

enum CW_PORT  { KEYBOARD, PADDLE_NORM, PADDLE_REVERSE, MANUAL, COOTIE };
extern volatile  CW_PORT CW_Port_Type;

#define IAMBICB 0x10 // 0 for Iambic A, 1 for Iambic B
extern volatile unsigned char keyerControl;
//during CAT commands, we will freeeze the display until CAT is disengaged
extern unsigned char doingCAT;


// Keyboard Commands:
#define KEY_IDLE   0
#define KEY_CMD    1
#define KEY_CW     'c'
#define KEY_BAND_DOWN   'd'
#define KEY_FREQ   'f'
#define KEY_HELP   'h'
#define KEY_LOCK   'l'
#define KEY_MODE   'm'
#define KEY_RIT    'r'
#define KEY_SPLIT  's'
#define KEY_TONE   't'
#define KEY_BAND_UP   'u'
#define KEY_VFO    'v'
#define KEY_WPM    'w'


#define MAX_KEYING_BUFFER 255
#define MAXKYBDENTRY 17


/**
 * Raduino needs to keep track of current state of the transceiver. These are a few variables that do it
 */
extern boolean txCAT;        //turned on if the transmitting due to a CAT command
extern volatile bool inTx;                //it is set to 1 if in transmit mode (whatever the reason : cw, ptt or cat)
extern int splitOn;             //working split, uses VFO B as the transmit frequency
extern volatile bool keyDown;             //in cw mode, denotes the carrier is being transmitted
extern char isUSB;               //upper sideband was selected, this is reset to the default for the 
                              //frequency when it crosses the frequency border of 10 MHz
extern byte menuOn;              //set to 1 when the menu is being displayed, if a menu item sets it to zero, the menu is exited
extern unsigned long cwTimeout;  //milliseconds to go before the cw transmit line is released and the radio goes back to rx mode
extern unsigned long dbgCount;   //not used now
extern unsigned char txFilter ;   //which of the four transmit filters are in use
extern boolean modeCalibrate;//this mode of menus shows extended menus to calibrate the oscillators and choose the proper
                              //beat frequency

/* these are functions implemented in the main file named as ubitx_xxx.ino */
void active_delay(int delay_by);
void saveVFOs();
void setFrequency(unsigned long f);
void startTx(byte txMode);
void stopTx();
void ritEnable(unsigned long f);
void ritDisable();
void cwKeyer(void);
void switchVFO(int vfoSelect, bool Display);

int enc_read(void); // returns the number of ticks in a short interval, +ve in clockwise, -ve in anti-clockwise
int btnDown(); //returns true if the encoder button is pressed

/* these functions are called universally to update the display */
void updateDisplay(); //updates just the VFO frequency to show what is in 'frequency' variable
void redrawVFOs();    //redraws only the changed digits of the vfo
void guiUpdate();     //repaints the entire screen. Slow!!
void drawCommandbar(char *text);
void drawTx();

//functions of the setup menu. implemented in seteup.cpp
void doSetup2(); //main setup function, displays the setup menu, calls various dialog boxes
void setupBFO();
void setupFreq();



//displays a nice dialog box with a title and instructions as footnotes
void displayDialog(char *title, char *instructions);
void printCarrierFreq(unsigned long freq); //used to display the frequency in the command area (ex: fast tuning)


//main functions to check if any button is pressed and other user interface events
void doCommands();  //does the commands with encoder to jump from button to button
void  checkTouch(); //does the commands with a touch on the buttons







/* these are functiosn implemented in ubitx_si5351.cpp */
void si5351bx_setfreq(uint8_t clknum, uint32_t fout);
void initOscillators();
void si5351_set_calibration(int32_t cal); //calibration is a small value that is nudged to make up for the inaccuracies of the reference 25 MHz crystal frequency 

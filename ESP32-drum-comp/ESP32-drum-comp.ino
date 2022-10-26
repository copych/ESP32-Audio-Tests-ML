/**************************************************************************
POLCA something like a Teenage Pocket Operater in the size of a Korg Volca

E.Heinemann 2021-06-04
Test of ESP32 with a lot of additional I2C-Components connecteed to SDA/SCL on GPIO 5/23
1. 3x PCF8574 I2C Muxer In/Out , PCF1&2 drive the LEDs and Step-Buttons, third PCF drives Control-Buttons and Rotary-Encoder
2. 1x ADS1115 Quad ADC
3. 1x SD1306 OLED
MIDI-Out/in, connected to GPIO 16/17
One LED on Pin 2

The Communication to I2C-devives is managed by Core 0 !!

2021-07-26 E.Heinemann - Midi Sync, ESP32 is a slave
2021-08-03 E.Heinemann - changed menu and removed some bugs
2021-08-04 E.Heinemann - Change of Soundsets and Delay with fixed values implemented
2021-08-14 E.Heinemann - Added Defines for SH1106 Display, - My SSD1306 was 0.96" and the SH1106 is 1.2"
2021-08-15 E.Heinemann - added Reverb Effect

**************************************************************************/

/**************************************************************************
Forked version by Copych, started 10/2022
Actually, I cut off most of the I2C stuff except OLED
16 pattern buttons connected via multiplexer and the rest -- directly to GPIOs, so buttons reading and processing reworked
also there's no more need to store a pattern as 2 x 8 bit words, it's a uint16_t now
**************************************************************************/
#define DEBUG_ON

#ifndef DEB
  #ifdef DEBUG_ON
  // found this hint with __VA_ARGS__ for macros on the web, it accepts variable sets of arguments /Copych
  #define DEB(...) Serial.print(__VA_ARGS__) 
  #define DEBF(...) Serial.printf(__VA_ARGS__) 
  #define DEBUG(...) Serial.println(__VA_ARGS__) 
  #else
  #define DEB(...)
  #define DEBF(...)
  #define DEBUG(...)
  #endif
#endif


#define BOARD_ESP32_DOIT


#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
//#include <MD_REncoder.h>
//#include <FastLED.h>
#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

#include <MIDI.h>       // MIDI-Library FoutysevenEffects

//#define USE_SSD1306
#define USE_SH1106

#ifdef USE_SSD1306
#include <Adafruit_SSD1306.h>
#endif

#ifdef USE_SH1106
#include <Adafruit_SH110X.h>
#endif

#define LED_PIN 2

#define ADC_BITS 12 
#define ADC_MAX ((1<<ADC_BITS)-1)
#define NORM127MUL float(127/ADC_MAX) 
#define LOGICAL_ON HIGH // HIGH means that a pressed button connects to the 3V3, while LOW would asume connecting to the GND
                        // please, note that some encoders have pull-up resistors onboard, and if you select HIGH, you need to 
                        // switch the polarity of such encoders, i.e. connect encoder's GND to 3V3 and encoder's "+" to GND
                        // this is quite safe since encoders shouldn't have active components and such reversing just pulls down

#define MUXED_BUTTONS 16 //number of multiplexed buttons
#define GPIO_BUTTONS 5   //number of ordinary GPIO buttons
#define TOTAL_BUTTONS MUXED_BUTTONS+GPIO_BUTTONS 

#define POT0 34         // potentiometers GPIOs
#define POT1 35
#define POT2 36
#define POT3 39

#define MUX_S0 14       // multiplexer CH-SELECT GPIOs
#define MUX_S1 27
#define MUX_S2 26
#define MUX_S3 25

#define MUX_Z  4        // multiplexer out (Z) GPIO

#define NAV0 13         // navigation (function) buttons
#define NAV1 15
#define NAV2 12
#define NAV3 2

#define ENC_SW 32       // encoder button (C)
#define ENC_CLK 33      // encoder clock  (A)
#define ENC_DT 23       // encoder data   (B)

#define I2S_BCLK_PIN  19
#define I2S_WCLK_PIN  18
#define I2S_DOUT_PIN  5

#define MIDIRX_PIN 16
#define MIDITX_PIN 17

#define SDA2 21         // OLED display pins
#define SCL2 22

// #define SDA1 21
// #define SCL1 22

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)

#define SAMPLE_RATE  38200
#define SAMPLE_SIZE_16BIT

enum e_pins_t     { fn1 = MUXED_BUTTONS,  fn2,  fn3,  fn4, enc_but }; // mnemonics for bits, starting with the first after the muxed buttons
uint8_t buttonGPIOs[GPIO_BUTTONS]{ NAV0, NAV1, NAV2, NAV3, ENC_SW  }; // GPIO buttons
//e_pins_t gpio_pin;

bool autoFireEnabled = false;                 // should the buttons generate continious clicks when pressed longer than a longPressThreshold
bool lateClickEnabled = false;                // enable registering click after a longPress call
const unsigned long longPressThreshold = 800; // the threshold (in milliseconds) before a long press is detected
const unsigned long autoFireDelay = 500;      // the threshold (in milliseconds) between clicks if autofire is enabled
const unsigned long riseThreshold = 20;       // the threshold (in milliseconds) for a button press to be confirmed (i.e. debounce, not "noise")
const unsigned long fallThreshold = 10;       // debounce, not "noise", also this is the time, while new "touches" won't be registered

// #define DEBUG_MAIN

#ifdef MIDITX_PIN
struct Serial2MIDISettings : public midi::DefaultSettings{
  static const long BaudRate = 31250;
  static const int8_t RxPin  = MIDIRX_PIN;
  static const int8_t TxPin  = MIDITX_PIN; // LED-Pin
};

HardwareSerial MIDISerial(2);

MIDI_CREATE_CUSTOM_INSTANCE( HardwareSerial, MIDISerial, MIDI, Serial2MIDISettings );
#endif
// remove resistors 70 and 68  on AI Thinker Audio 2.2 - Board to free up Pin 5 and Pin 23!

// TwoWire I2Cone = TwoWire(0);
TwoWire I2Ctwo = TwoWire(0);

#ifdef USE_SSD1306
#define SCREEN_ADDRESS 0x3C // The Address was discovered using the I2C Scanner
Adafruit_SSD1306 display( SCREEN_WIDTH, SCREEN_HEIGHT, &I2Ctwo, OLED_RESET );
#endif

#ifdef USE_SH1106
#define SCREEN_ADDRESS 0x3C // The Address was discovered using the I2C Scanner
Adafruit_SH1106G display=  Adafruit_SH1106G( SCREEN_WIDTH, SCREEN_HEIGHT, &I2Ctwo );
#endif


uint16_t step_pattern = 0xFFFF; // bitmask for an unmodified pattern - stored for the instrument...

// Steps are stored in pcf_value1 and pcf_value2 active-Bit is deactivated step, low bit is a activated step! Inverse Logic!!
uint16_t pcf_value1;

// Values which are used to display
// old-values
uint16_t pcf_value1_1;




boolean  external_clock_in = false;
float    bpm = 90.5;
float    old_bpm = 90.5;
uint8_t  bpm_pot_midi =(int) ( bpm - 30 ) / 2; // range 30 <-> 285 bpm mit MIDI_Values 0-127
uint8_t  bpm_pot_midi_old = bpm_pot_midi;

uint8_t  bpm_pot_fine_midi = ( bpm - ( bpm_pot_midi*2 + 30 ))*20 + 65; // range ist zu testen ...
uint8_t  bpm_pot_fine_old_midi = bpm_pot_fine_midi;

uint8_t  swing_midi = 0; // ToDo for later time-swing or accent-pattern

// Soundset/Program-Settings
uint8_t  program_midi =0; // 5 Programs
uint8_t  program_tmp = 0; 
uint8_t  progNumber = 0; // first subdirectory in /data 
uint8_t  countPrograms = 5;


// Delay-Settings
uint8_t  delayToMix_midi = 65;
uint8_t  delayFeedback_midi = 10;
uint8_t  delayLen_midi = 127;
uint8_t  delayToMix_old = 65;
uint8_t  delayFeedback_old = 10;
uint8_t  delayLen_old = 127;

// Reverb-Settings
uint8_t rev_level_midi = 10;
uint8_t rev_level_old = 10;


// Accent/Velocity Settings
uint16_t count_ppqn = 0;
uint8_t  veloAccent = 120;
uint8_t  veloAccent_midi = 120;
uint8_t  veloInstr = 100;
uint8_t  veloInstr_midi = 100;
uint8_t  midi_channel = 10;

uint8_t  count_bars_midi=127; // 16 Bars
uint8_t  count_bars = 16;

boolean  func1_but_pressed = false; // left blue
boolean  func2_but_pressed = false; // upper blue for global menus
boolean  func3_but_pressed = false; // 

// Test for Display
String   active_track_name="LCO";
uint8_t  active_track_num = 1;
uint16_t active_step = 0; 
// 96 steps per full note, 
// 24 per quarter note 
// 12 for eighth notes, 6 for 16th notes ... Stepsequencer works with 16th notes, therefore 6 clock signals from one step to the next.

// this is used to add a task to core 0
TaskHandle_t  Core0TaskHnd;

// These values are only used to make an integration with MIDI, additional analog inputs or with an Menu slightly simplier
// The values could be 0 - 127 or floats ... 
uint8_t global_playbackspeed = 64; // The Playbackspeed
uint8_t global_bitcrush = 0;       // is bitcrusher active
uint8_t global_biCutoff = 64;      // Cutoff-Frequency of the filter
uint8_t global_biReso= 64;         // Resonance of the filter

uint8_t global_playbackspeed_midi= 64; // The Playbackspeed
uint8_t global_bitcrush_midi= 0;       // is bitcrusher active
uint8_t global_biCutoff_midi= 64;      // Cutoff-Frequency of the filter
uint8_t global_biReso_midi= 64;        // Resonance of the filter

uint8_t global_playbackspeed_midi_old = global_playbackspeed_midi; // The Playbackspeed
uint8_t global_bitcrush_midi_old = global_bitcrush_midi;           // is bitcrusher active
uint8_t global_biCutoff_midi_old = global_biCutoff_midi;           // Cutoff-Frequency of the filter
uint8_t global_biReso_midi_old = global_biReso_midi;               // Resonance of the filter

uint8_t global_pitch_decay_midi = 64;
uint8_t global_pitch_decay_midi_old = 64; // 64 = global_pitch_decay = 0.0f !

uint8_t act_menuNum = 0;
uint8_t act_page = 0;
uint8_t act_instr = 1; // Pad1 Changed by Rotary Encoder

// Menu
String   menus[] = { "Sound 1", "Sound 2", "Filter",  "Delay", "Soundset", "Tempo", "Load", "Save", "Sync"};
uint8_t  menuNum[] = { 0,          1,         2,          3 ,      4,          5,       6 ,    7,       8 };
 
String  act_menu = menus[ act_menuNum ];

uint8_t act_menuNum_max = 8;

// Instrument 0 is Accent and has a differenz Menu!
String pages_accent_short[]={"Acc","Ins","Vac","-"}; // Vac = Vactrol Audio Control via LDR   

// Standard-Menus
String pages_short[] = { "Vol", "Dec", "Pit","Pan"
  , "Not", "Cha", "Att","EG"
  , "Frq", "Res", "Bit","PCM"
  , "Mix", "Fbk", "Len", "Rvb"  
  , "Set", "-", "-", "-"
  , "Tmp", "Fin", "Steps", "Swi"
  , "Load", "-", "-", "-"
  , "Save", "-", "-", "-"
  , "SyncIn", "SyncOut", "-", "-"
   };

String no_display = "-";  

// Is the Sequencer running or not
boolean  playBeats = false;

// Instruments, Accent is not an Instrument but internally handled as an instrument .. therefore 17 Instruments from 0 to 16
// const 
String shortInstr[] ={ "ACC"   , "111", "222" , "333" , "HHop", "Cr", "Cl", "LT", "HT", "S1", "S2", "S3", "S4"
                       ,"T1","T2","T3","T4" };
const String instrument[]      ={ "Accent", "S1", "S2" , "S3" , "S4", "S5", "S6", "S7", "S8", "S9", "S10", "S11", "S12","S13","S14","S15","S16" };
int8_t  midinote_in_out[] ={ -1,  36, 37, 38, 39, 40, 41, 42, 43,  44, 45, 46, 47, 48, 49, 50, 51 }; // MIDI-Sound, edited via Menu 50=TOM, 44=closed HH, 
int8_t midichannel_in_out[] ={ -1,  10,10,10,10, 10,10,10,10, 10,10,10,10, 10,10,10,10 }; // MIDI-Channel je Sound, edited via Menu 50=TOM, 44=closed HH, 

// uint8_t iVelo[]  ={ 127, 90, 90, 90, 90, 90, 90, 90, 90,  90, 90, 90, 90, 90, 90, 90, 90 }; // Velocity, edited via Menu
//uint8_t inotes[]= { 254, 254,238,85 ,255,255,255,255,255, 255,255,255,255,255,255,255, 255 };
//uint8_t inotes2[]={ 254, 254,255,255,255,255,255,255,255, 255,255,255,255,255,255,255, 255 };
uint16_t inotes[]={0xfefe, 0xfefe,0xeeff,0x55ff,0xffff,0xffff,0xffff,0xffff,0xffff, 0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff, 0xffff};
// Incerse Logic Step1 is Bit0, set it to 0 to activate it
uint8_t patterns_onbeat[]  = { 255, 254, 110, 190, 230, 239, 42, 142, 255 }; // last record ( 255 ) to store the current Pattern
uint8_t patterns_offbeat[] = { 255, 251, 187, 219, 238, 85, 155, 153, 255 }; // last record ( 255 ) to store the current Pattern

uint8_t current_pattern = 0;

boolean is_muted[]={ false, false,false,false,false ,false,false,false,false ,false,false,false,false ,false,false,false,false };
                    
uint8_t volume_midi[]     = { 127, 127,127,127,127, 127,127,127,127, 127,127,127,127, 127,127,127,127 };
uint8_t attack_midi[]     = { 0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };
uint8_t decay_midi[]      = { 100, 100,100,100,100, 100,100,100,100, 100,100,100,100, 100,100,100,100  };
uint8_t pitch_midi[]      = { 64, 64,64,64,64, 64,64,64,64, 64,64,64,64, 64,64,64,64 };
uint8_t pan_midi[]        = { 64, 64,64,64,64, 64,64,64,64, 64,64,64,64, 64,64,64,64 };
uint8_t pitchdecay_midi[] = { 64, 64,64,64,64, 64,64,64,64, 64,64,64,64, 64,64,64,64 };

// float step_delay[] ={ 10.1,10.1,10.1,10.1, 10.1,10.1,10.1,10.1, 10.1,10.1,10.1,10.1, 10.1,10.1,10.1,10.1  ,10.1 };
uint8_t count_instr = 17;

// We get some values for parameters from a Patchmanager
// param_valX is the current value
uint16_t param_val0, param_val1, param_val2, param_val3;
// patch_val0 is the value from the patchmanager
// uint8_t patch_val0, patch_val1, patch_val2, patch_val3;

// First Values for the first Menu and selected Instrument
uint16_t patch_val0 = volume_midi[ act_instr ];
uint16_t patch_val1 = decay_midi[ act_instr ];
uint16_t patch_val2 = pitch_midi[ act_instr ];
uint16_t patch_val3 = pan_midi[ act_instr ];

// Some Names for the values - only for testing
String param_name0 = pages_short[0];//  "Vol"; 
String param_name1 = pages_short[1]; 
String param_name2 = pages_short[2]; 
String param_name3 = pages_short[3]; 

int16_t adc0, adc1, adc2, adc3;
int16_t adc0_1, adc1_1, adc2_1, adc3_1;

uint8_t adc_slope = 15;

boolean do_display_update = false;
boolean do_display_update_fast = false;

float volts0, volts1, volts2, volts3;

uint16_t display_prescaler = 0;
uint16_t display_prescaler_fast = 0;
uint16_t midi_prescaler   = 0; 
uint16_t ads_prescaler    = 0; 
uint16_t patch_edit_prescaler = 0; // Wait seconds after loading to sync the values

uint8_t val0_synced = 0; // 0 adc-value is lower, 2 adc-value is higher, 1 = values are synced
uint8_t val1_synced = 0; // 0 adc-value is lower, 2 adc-value is higher, 1 = values are synced
uint8_t val2_synced = 0; // 0 adc-value is lower, 2 adc-value is higher, 1 = values are synced
uint8_t val3_synced = 0; // 0 adc-value is lower, 2 adc-value is higher, 1 = values are synced

int16_t val_dev_null = 0;

uint8_t compare2values( uint16_t val1, uint16_t val2 ){
  if( val1 < val2 ) return 0;
  if( val1 > val2 ) return 2;
  return 1;
}

// ####
// ####   SETUP    ####
// ####
void setup(){
  // Serial
  #ifdef DEBUG_ON
  Serial.begin( 115200 );
  while (!Serial);
  Serial.println( __FILE__ );
  #endif
#ifdef MIDIRX_PIN
  // MIDI on Core 1 - the default-Core for Arduino
  pinMode( MIDIRX_PIN , INPUT_PULLUP); 
  MIDISerial.begin( 31250, SERIAL_8N1, MIDIRX_PIN, MIDITX_PIN ); // midi port
  MIDI_setup();
#endif
  
#if 0
  setup_wifi();
#else
  WiFi.mode(WIFI_OFF);
#endif
  btStop();
  
  setup_i2s();
  Sampler_Init();
  Effect_Init();
  Effect_SetBitCrusher( 0.0f );
  Reverb_Setup();  
  Delay_Init();  

  xTaskCreatePinnedToCore( Core0Task, "Core0Task", 8000, NULL, 5, &Core0TaskHnd, 0);

  step_pattern = inotes[ act_instr ];
  sequencer_new_instr( act_instr );

  DEBF( "ESP.getFreeHeap() %d\n", ESP.getFreeHeap() );
  DEBF( "ESP.getMinFreeHeap() %d\n", ESP.getMinFreeHeap() );
  DEBF( "ESP.getHeapSize() %d\n", ESP.getHeapSize() );
  DEBF( "ESP.getMaxAllocHeap() %d\n", ESP.getMaxAllocHeap() );
  
}

long myTimer = millis();
long myTimer_bar = millis();
long cur_time = millis();
long myTimer_Delta = sequencer_calc_delay( bpm );

// ###
// ### Audio-Task ###
// ###
float fl_sample = 0.0f;
float fr_sample = 0.0f;

inline void audio_task(){
    /* prepare out samples for processing */
   fl_sample = 0.0f;
   fr_sample = 0.0f;

   Sampler_Process( &fl_sample, &fr_sample );
   Effect_Process( &fl_sample, &fr_sample );
   Delay_Process(&fl_sample, &fr_sample);
   Reverb_Process(&fl_sample, &fr_sample );

    //Don’t block the ISR if the buffer is full
    if( !i2s_write_samples(fl_sample, fr_sample )){
        DEBUG("i2s output error");
    }
}

void sync_compared_values(){
    val0_synced = compare2values( param_val0, patch_val0 );    
    val1_synced = compare2values( param_val1, patch_val1 );    
    val2_synced = compare2values( param_val2, patch_val2 );    
    val3_synced = compare2values( param_val3, patch_val3 );    
}

// ###
// ### Core 0 Loop ###
// ###
int16_t act_instr_tmp =  act_instr;
uint16_t tmp_x = 0;
uint16_t tmp_bpm = 130;

void Core0TaskLoop(){
  // put your loop stuff for core0 here
   
  processEncoder();
  
  ads_prescaler +=1;
  
  if( ads_prescaler > 70 ){
  processButtons();    // process buttons: generating onTouch(), onPress(), onLongPress(), onAutoClick(), onClick() and onRelease() calls 
  adcRead ( 0 , param_val0, param_val1, param_val2, param_val3 );
  }

  display_prescaler +=1;
  if(( do_display_update== true && display_prescaler > 6 ) || display_prescaler > 170  ){
    update_display_bars();
    display_prescaler = 0;
    do_display_update = false;
    do_display_update_fast = false;
  }

  if( playBeats==true ){
    pcf_update_leds();
    // do_display_update_fast = false;
    
    // display_prescaler_fast = 0;
    //}
  }

  // How is the status after loading a new patch, this should be done only once!!
  if( patch_edit_prescaler == 25 ){
    sync_compared_values();   
  }

  // How is the status after a while, after the value crossed its old value, the value should be in sync!
  if( patch_edit_prescaler > 50 && (  val0_synced != 1 || val1_synced != 1 || val2_synced != 1 || val3_synced != 1 ) ){
    if( val0_synced != 1 && compare2values( param_val0, patch_val0 ) != val0_synced ) val0_synced = 1;
    if( val1_synced != 1 && compare2values( param_val1, patch_val1 ) != val1_synced ) val1_synced = 1;
    if( val2_synced != 1 && compare2values( param_val2, patch_val2 ) != val2_synced ) val2_synced = 1;
    if( val3_synced != 1 && compare2values( param_val3, patch_val3 ) != val3_synced ) val3_synced = 1;
  }else{
    patch_edit_prescaler +=1;  
  }

}

// ###
// ### Core 0 Setup ###
// ### used for hardware-UI
// ###
void Core0TaskSetup(){
  // INIT ADC
  analogReadResolution(ADC_BITS); // bit resolution ( 9..12 )
  analogSetWidth(ADC_BITS);
  pinMode( POT0, INPUT );
  pinMode( POT1, INPUT );
  pinMode( POT2, INPUT );
  pinMode( POT3, INPUT );

#if MUXED_BUTTONS > 0
  // INIT multiplexer pins
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S3, OUTPUT);
  pinMode(MUX_Z,INPUT_PULLDOWN);
#endif
  //  GPIO buttons 
  for (uint8_t i = 0; i < GPIO_BUTTONS; i++) {
    pinMode(buttonGPIOs[i], INPUT_PULLDOWN);
  }
  pinMode( ENC_DT, INPUT_PULLDOWN); 
  pinMode( ENC_CLK, INPUT_PULLDOWN); 

  //Display
  I2Ctwo.begin( SDA2, SCL2, uint32_t(200000) ); // SDA2 pin 0, SCL2 pin 5, works with 50.000 Hz if pinned to other Core0
  // SSD1306 OLED - Display first to be able to show Errors on the Display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
#ifdef USE_SSD1306
  if( !display.begin( SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS) ){
    DEBUG("SSD1306 allocation failed");
    for(;;); // Don't proceed, loop forever
  }
#endif

#ifdef USE_SH1106
 // 0x3c
  if( !display.begin( SCREEN_ADDRESS, true) ){
    DEBUG("SH1106 allocation failed");
    for(;;); // Don't proceed, loop forever
  }
#endif

}

void Core0Task( void *parameter ){
  Core0TaskSetup();
  while( true ){
    Core0TaskLoop();
    // this seems necessary to trigger the watchdog
    delay(1);
    yield();
  }
}



// ###
// ### Default - Loop ###
// ### Audio, Sequencer, MIDI
// ###
void loop() {
  audio_task();

  // Stepsequencer
  cur_time =  millis();
  if( external_clock_in == false ){
    if( playBeats && myTimer_Delta + myTimer < cur_time ){  
      myTimer_bar = myTimer_Delta + myTimer;
      sequencer_callback();
 
    //  DEB("Time-Difference Clock:");
    //  DEBUG( cur_time - ( myTimer_Delta + myTimer ) ); 
        
      Delay_Sync_Values();
      Effect_Sync_Values();
      Reverb_Sync_Values();
      myTimer = myTimer + myTimer_Delta;
    }
  } else {
    if( myTimer_Delta + myTimer < cur_time ){  
       Delay_Sync_Values();
       Effect_Sync_Values();
       Reverb_Sync_Values();
       myTimer = myTimer + myTimer_Delta;   
    }
  }
    
/*
    
    // only for MIDI-Clock, the for step 0 and not that precise
    if( playBeats && ( count_ppqn > 0 || ( count_ppqn==0 && active_step==0 ))  && myTimer_Delta + myTimer < cur_time ){  
      if( active_step==0 && count_ppqn==0 ){
        //myTimer_bar = myTimer_bar + step_delay[ 16 ];// myTimer_Delta + myTimer;
        myTimer_bar = myTimer_Delta + myTimer;
      } 
      sequencer_callback();

      DEB("Time-Difference Clock:");
      DEBUG( cur_time - ( myTimer_Delta + myTimer ) ); 
 
      myTimer = myTimer + myTimer_Delta;
    }
    
    // Triggering the Sounds and a bit more precise
    if( playBeats && count_ppqn == 0 && active_step > 0 && myTimer_bar + step_delay[ active_step ] < cur_time ){  
      myTimer = myTimer_bar + step_delay[ active_step ] + myTimer_Delta;
      sequencer_callback();
 
      DEB("Time-Difference Precise:");
      DEBUG( cur_time - ( myTimer_Delta + myTimer ) ); 
     
    }  
*/
    
  
  // MIDI  
  midi_prescaler++;
  if( midi_prescaler >= 2 ){
      MIDI_Process();
      midi_prescaler = 0;
  }

}

// 2021-07-05 E.Heinemann, this file covers all functions of the sequencer. probably I have to create the sequencer and tracks as classes - perhaps as a structure.
// 2021-08-03 E.Heinemann, changed Accent-Velocity and normal velocity

// #define DEBUG_SEQUENCER
// #define DEBUG_SEQUENCER_TIMING


// Scale, Menu to change Scale is yet not implemented
uint8_t velocity = 90;
uint8_t scale=1 ;//Default scale  1/16
uint8_t scale_value[]       = {     2,      5,    12,     24,       8,       16 };
const String scale_string[] = { "1/32", "1/16", "1/8", "1/4",  "1/8T",   "1/4T" };

// toggles Mute
void sequencer_mute_instr(){
  //  act_instr
  is_muted[ act_instr ] = ! is_muted[ act_instr ];
}

// toggles Solo
void sequencer_solo_instr(){
  //  act_instr
}

void sequencer_new_instr( uint8_t new_instr_num  ){

  // store the values of the last instrument
  inotes[ act_instr ] = step_pattern;
  
  //DEB("old_step_pattern: " );
  //DEBUG( step_pattern );
  
  // get the stored values of the new instrument  
  act_instr = new_instr_num;
  active_track_num = act_instr;
  active_track_name = shortInstr[ act_instr ];
    
  step_pattern = inotes[ act_instr ];
 
  //DEB("new_step_pattern: ");
  //DEBUG( step_pattern );

  // Poti-Values synchronisieren
  patch_edit_prescaler = 0;
  if( act_instr > 0 ){
    // Volume
    param_val0 = volume_midi[act_instr-1]; // Sampler_GetSoundVolume_Midi();  
    // Decay
    param_val1 = decay_midi[act_instr-1];
    // Pitch 
    param_val2 = pitch_midi[act_instr-1];
    // Panorama
    param_val3 = pan_midi[act_instr-1];// Sampler_GetSoundPan_Midi();
  }else{
    // Accent
    param_val0 = veloAccent_midi;
    param_val1 = veloInstr_midi;    
  }
  step_bits = step_pattern;

#ifdef DEBUG_SEQUENCER 
  DEB("new Instrument: " );
  DEB( active_track_num );
  DEB( " " );
  DEBUG( active_track_name );
#endif  
  do_display_update = true;
  
}

float sequencer_calc_delay( float new_bpm ){
  // Delay for count_ppqn and a Array for the Step-Timer
  float tempo_delay = 60000 / ( new_bpm * scale_value[scale] * 4 ); // scale-value provides the count of Clock-Ticks per 4th note ..  normally 16th equals 6 steps * 4 to get   
  // calculate the delay for every step of the 16 steps
  // TODO: This is the right place to insert some kind of "Swing"
  
/* // This will be a function to create Swing
  for( int i=0; i <= 16; i++ ){
    step_delay[i] = i * tempo_delay * scale_value[scale];
#ifdef DEBUG_SEQUENCER_TIMING    
    DEB("step_delay: ");
    DEB( i );
    DEB( " " );
    DEBUG( step_delay[i] );
#endif
  }
*/
  
#ifdef DEBUG_SEQUENCER    
  DEB("tempo_delay: ");
  DEBUG( tempo_delay );
#endif  

  return tempo_delay;
}

// #### 
// #### Start Sequencer
// ####
void sequencer_start(){
  active_step = 0; // 0 -- 15
  count_ppqn = 0;
  if( external_clock_in == false ){
    myTimer_Delta = sequencer_calc_delay( bpm );
    myTimer = millis() - myTimer_Delta;
    myTimer_bar = myTimer;
  }
#ifdef DEBUG_SEQUENCER
  DEBUG("Start");
#endif
  // MIDI-Clock
  playBeats = true;
  #ifdef MIDITX_PIN
  MIDI.sendRealTime( MIDI_NAMESPACE::Start );
  #endif
}


// #### 
// #### Stop Sequencer
// ####
void sequencer_stop(){
  // Stop-Button was pressed or MIDI-Stop received
  // Der Step darf nicht auf 0 gesetzt werden, denn es könnte ein Continue erfolgen
  // active_step=0;
  playBeats = false;
#ifdef DEBUG_SEQUENCER
  DEBUG("Stop");
#endif
  // lcd.setCursor(0,1);
  // lcd.print( "Stopped Beating ");
#ifdef MIDITX_PIN
  MIDI.sendRealTime(MIDI_NAMESPACE::Stop);
#endif
}

// #### 
// #### Continue Sequencer
// ####
void sequencer_continue(){
  if( playBeats == false ){
    myTimer_Delta = sequencer_calc_delay( bpm );
    myTimer = millis() - myTimer_Delta;
    myTimer_bar = myTimer; // is wrong and must be calculated based on the current step and ppqn!
    playBeats = true;
#ifdef DEBUG_SEQUENCER
    DEBUG("Continue");
#endif
    // MIDI-Clock
    // MIDI.sendRealTime( MIDI_NAMESPACE::Start ); 
  }
}


// ####
// #### Show Notes and current Step via LEDs  ######
// ####
void showStep( uint8_t notes1  ){
  uint8_t bitdp1 = notes1;
  
  // Current Step would be only shown if played
  if( playBeats==true ){ 
      bitClear( bitdp1, active_step ); 
  }
  // should better be a function in the module PCF!!
//  PCF1.write8( bitdp1 );
}

// ####
// #### Update the Notes of the current Instrument ##### 
// ####
void sequencer_update_track( byte step_bits_old ){
  inotes[ act_instr ] =  step_bits_old;
}


// ####
// #### Play the Midi-Notes of the actual active_step ##### 
// ####
void sequence_process(){
  // first 8 beats
  if( playBeats==true ){
       
#ifdef DEBUG_SEQUENCER    
    DEBUG("sequence_process");
#endif    
    velocity = veloInstr_midi; // Normal Velocity by default
    // first Byte or first 8 Hits
    // "song_position" is the current step 
  
    if( active_step<16 ){ // play notes1
      // Accent set?
      if( bitRead( inotes[0], active_step ) == 0 ){
        // Accent is set
        velocity = veloAccent_midi; // iVelo[0];
        if( velocity > 127 ){
          velocity = 127;
        }
      } 

      // loop through all instruments .. But ignore Accent with 0       
      for( int i = 1; i < count_instr; i++ ){
        if( bitRead( inotes[i], active_step ) == 0 ){
#ifdef MIDITX_PIN
          MIDI.sendNoteOff( midinote_in_out[i],         0, midichannel_in_out[i]-1 ); // MIDI-Off could be removed if You trigger external analog gear. For Samplers you should keep it     
          MIDI.sendNoteOn(  midinote_in_out[i], velocity , midichannel_in_out[i]-1 );  // iSound is the array with the MIDI-Note-Number for the Pads
#endif
          Sampler_NoteOn( i-1, velocity ); // Accent "0" has no Sample to play
          
#ifdef DEBUG_SEQUENCER 
          DEBUG("NoteOn");
#endif
         }  
       }
       
    }

    // if we do it with more than 16 Steps , they have to be defined here    
  } 
}

// ####
// #### Callback from Timer1
// ####
void sequencer_callback(){
  if( playBeats == true ){
    if( external_clock_in == false && old_bpm != bpm ){
      // Timer1 is missing on ESP32  !!
      myTimer_Delta = sequencer_calc_delay( bpm );
      myTimer = millis() - myTimer_Delta;
      myTimer_bar = myTimer;
      old_bpm = bpm;
    }
      
    if( count_ppqn ==0 ){
      sequence_process();
      digitalWrite( LED_PIN, HIGH ); 
  #ifdef DEBUG_SEQUENCER  
      DEB("sequencer_callback() "); 
      DEB( count_ppqn ); 
      DEB( " " ); 
      DEBUG( active_step );
  #endif
    }
    
    if( count_ppqn >= scale_value[scale] ){
      active_step++;
      if( active_step >= count_bars ){
        active_step = 0;
      }
      count_ppqn =0;
  
 
      do_display_update_fast = true;
    }else{
      count_ppqn++;  
      digitalWrite( LED_PIN, LOW );
    } 
  }
}

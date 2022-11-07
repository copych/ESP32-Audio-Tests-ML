void readButtonsState(uint32_t &activeFlags) {
  activeFlags=0;
#if MUXED_BUTTONS > 0
  // assuming that first 16 buttons are connected via 16 ch multiplexer
  for (uint8_t i = 0; i < MUXED_BUTTONS; i++) {
    // if (readMuxed(i)) {bitSet(activeFlags, i);} else {bitClear(activeFlags, i);}
    bitWrite(activeFlags, i, readMuxed(i));
  }
#endif
  // all the rest are just GPIO buttons 
  for (uint8_t i = 0; i < GPIO_BUTTONS; i++) {
    bitWrite(activeFlags, i + MUXED_BUTTONS, digitalRead(buttonGPIOs[i]) == LOGICAL_ON);
  }
}

#if MUXED_BUTTONS>0
bool readMuxed(uint8_t channel) { 
  // select the multiplexer channel
  digitalWrite(MUX_S0, bitRead(channel, 0));
  digitalWrite(MUX_S1, bitRead(channel, 1));
  digitalWrite(MUX_S2, bitRead(channel, 2));
  digitalWrite(MUX_S3, bitRead(channel, 3));
  delayMicroseconds(1);
  // read channel: LOGICAL_ON should be defined as HIGH or LOW depending on hardware settings (PullHigh or PullLow)
  return (digitalRead(MUX_Z) == LOGICAL_ON); 
}
#endif

void processEncoder() { // idea was taken from Alex Gyver's examples
  static uint8_t newState = 0;
  static uint8_t oldState = 0;
 // static const int8_t stepIncrement[16] = {0, 1, -1, 0,  -1, 0, 0, 1,  1, 0, 0, -1,  0, -1, 1, 0}; // Half-step encoder
  static const int8_t stepIncrement[16] = {0, 0, 0, 0,  -1, 0, 0, 1,  1, 0, 0, -1,  0, 0, 0, 0}; // Full-step encoder

  uint8_t clk = digitalRead(ENC_CLK)==LOGICAL_ON;
  uint8_t dt = digitalRead(ENC_DT)==LOGICAL_ON;
  DEB(clk);
  DEB("\t");
  DEBUG(dt);
  newState = (clk | dt << 1);
  if (newState != oldState) {
    int8_t stateMux = newState | (oldState << 2);
    int8_t rotation = stepIncrement[stateMux];
    oldState = newState;
    if (rotation != 0) {
      encoderMove(rotation);
    }
  }
}

void processButtons() {
  static uint32_t buttonsState = 0;                           // current readings
  static uint32_t oldButtonsState = 0;                        // previous readings
  static uint32_t activeButtons = 0;                          // activity bitmask
  static unsigned long currentMillis;                         // not to call repeatedly within loop
  static unsigned long riseTimer[TOTAL_BUTTONS];              // stores the time that the button was pressed (relative to boot time)
  static unsigned long fallTimer[TOTAL_BUTTONS];              // stores the duration (in milliseconds) that the button was pressed/held down for
  static unsigned long longPressTimer[TOTAL_BUTTONS];              // stores the duration (in milliseconds) that the button was pressed/held down for
  static unsigned long autoFireTimer[TOTAL_BUTTONS];          // milliseconds before firing autoclick
  static bool buttonActive[TOTAL_BUTTONS];                    // it's true since first touch till the click is confirmed
  static bool pressActive[TOTAL_BUTTONS];                     // indicates if the button has been pressed and debounced
  static bool longPressActive[TOTAL_BUTTONS];                 // indicates if the button has been long-pressed
  
  readButtonsState(buttonsState);                    // call a reading function
  
  for (uint8_t i = 0 ; i < TOTAL_BUTTONS; i++) {
    if ( bitRead(buttonsState, i) != bitRead(oldButtonsState, i) ) {
      // it changed
      if ( bitRead(buttonsState, i) == 1 ) {                  // rise (the beginning)
        // launch rise timer to debounce
        if (buttonActive[i]) {                                // it's active, but rises again
          // it might be noise, we check the state and timers
          if (pressActive[i]) {
            // it has passed the rise check, so it may be the ending phase
            // or it may be some noise during pressing
          } else {
            // we are still checking the rise for purity
            if (millis() - riseTimer[i] > riseThreshold) {
              // not a good place to confirm but we have to
              pressActive[i] = true;
            }
          }
        } else {
          // it wasn't active lately, now it's time
          buttonActive[i] = true; // fun begins for this button
          riseTimer[i] = millis();
          longPressTimer[i] = millis(); // workaround for random longPresses
          autoFireTimer[i] = millis(); // workaround 
          bitWrite(activeButtons, i, 1);
          onTouch(i, activeButtons); // I am the first          
        }
      } else {                                                // fall (is it a click or just an end?)
        // launch fall timer to debounce
        fallTimer[i] = millis();
       // pendingClick[i] = true;
      }
    } else {                                                  // no change for this button
      if ( bitRead(buttonsState, i) == 1 ) {                  // the button reading is "active"
        // someone's pushing our button
        if (!pressActive[i] && (millis() - riseTimer[i] > riseThreshold)) {
          pressActive[i] = true;
          longPressTimer[i] = millis();
          onPress(i, activeButtons);
        }
        if (pressActive[i] && !longPressActive[i] && (millis() - longPressTimer[i] > longPressThreshold)) {
          longPressActive[i] = true;
          onLongPress(i, activeButtons);
        }
        if (autoFireEnabled && longPressActive[i]) {
          if (millis() - autoFireDelay > autoFireTimer[i]) { 
            autoFireTimer[i] = millis();
            onAutoClick(i, activeButtons);                    // function to execute on autoClick event 
          }
        }
      } else {                                                // the button reading is "inactive"
        // ouch! a click?
        if (buttonActive[i] && (millis() - fallTimer[i] > fallThreshold)) {
          // yes, this is a click
          buttonActive[i] = false; // bye-bye
          pressActive[i] = false;
          if (!longPressActive[i] || lateClickEnabled) {
            onClick(i, activeButtons);
          }
          longPressActive[i] = false;
          onRelease(i, activeButtons);
        }
      }
    }
    bitWrite(activeButtons, i, buttonActive[i]);
  }
  oldButtonsState = buttonsState;
}


// buttonNumber - variable that indicates the button caused the event
// activeButtonsBitmask flags all the active buttons 
// each binary digit contains a state: 0 = inactive, 1 = active
void onTouch (uint8_t buttonNumber, uint32_t activeButtonsBitmask) {
  DEB("Flags: ");
  DEB(activeButtonsBitmask);
  DEB(" Touch: ");
  DEBUG(buttonNumber);
}
  
void onPress (uint8_t buttonNumber, uint32_t activeButtonsBitmask) {
  DEB("Flags: ");
  DEB(activeButtonsBitmask);
  DEB(" Press: ");
  DEBUG(buttonNumber);
  if (buttonNumber >= MUXED_BUTTONS){
    switch (buttonNumber) {
      case fn1:
        func1_but_pressed = true;   // left blue
          if( act_menuNum > 1 ){ 
            changeMenu( 0 );
          }
        break;
      case fn2:       // upper blue for global menus
        func2_but_pressed = true; 
        break;
      case fn3:
        //func3_but_pressed = true; 
        // Play?
        if( playBeats == false ){
          sequencer_start();  
          external_clock_in = false;
          DEBUG("PLAY");
        }else{
          sequencer_stop();
          external_clock_in = false;
          // sequencer_start();
          DEBUG("STOP");
        }
        break;
      case fn4:
        // Continue?
        if( playBeats == false ){
          sequencer_continue();
          external_clock_in = false;
          DEBUG("CONTINUE");
        }else{
          sequencer_stop();
          external_clock_in = false;
          DEBUG("STOP");
        }
      case enc_but:
        // Shortcut Func1 + Extra Button to adopt all values from the pots?
        if( func1_but_pressed = true && ( act_menuNum == 0 || act_menuNum == 1 || act_menuNum  == 2 ) ){
          val0_synced = 1;
          val1_synced = 1;
          val2_synced = 1;
          val3_synced = 1;
        }
        break;
    }
  } else {
    if( func1_but_pressed == false && func2_but_pressed == false ){
      bitWrite( step_pattern, buttonNumber, 1 - bitRead(step_pattern, buttonNumber) );
    }else{
      if( func1_but_pressed == true && func2_but_pressed == false ){
        func1_but( buttonNumber );
      }else{
        func2_but( buttonNumber );
      }
    }
  }
  step_bits = step_pattern;

   
  if( do_display_update == false && ( step_bits != step_bits_old   ) ){
    do_display_update = true;
  }

  step_bits = step_pattern; 
  step_bits_old = step_bits; 

  // Values auf die Bytes des aktuellen Instruments übertragen!!
  // From UI-Core to Sequencer-Core

  sequencer_update_track( step_pattern );
  
  // activate LEDs, but if the beat is playing, invert the current step
  pcf_update_leds();
}
  
void onClick (uint8_t buttonNumber, uint32_t activeButtonsBitmask) {
  DEB("Flags: ");
  DEB(activeButtonsBitmask);
  DEB(" Click: ");
  DEBUG(buttonNumber);
}
  
void onLongPress (uint8_t buttonNumber, uint32_t activeButtonsBitmask) {
  DEB("Flags: ");
  DEB(activeButtonsBitmask);
  DEB(" Long Press: ");
  DEBUG(buttonNumber);
}
  
void onAutoClick (uint8_t buttonNumber, uint32_t activeButtonsBitmask) {
  DEB("Flags: ");
  DEB(activeButtonsBitmask);
  DEB(" AutoFire: ");
  DEBUG(buttonNumber);
}

void onRelease (uint8_t buttonNumber, uint32_t activeButtonsBitmask) {
  DEB("Flags: ");
  DEB(activeButtonsBitmask);
  DEB(" Release: ");
  DEBUG(buttonNumber);
  switch (buttonNumber) {
    case fn1:
      func1_but_pressed = false; // left blue
      break;
    case fn2:
      func2_but_pressed = false; // upper blue for global menus
      break;
    case fn3:
      func3_but_pressed = false; //
      break;
  }
}

void encoderMove (int8_t rotation) {
//  DEB("Encoder: ");
//  DEBUG(rotation);
    // Check Rotary-Encoder-Movements
  switch( act_menuNum ){
     case 0:
      act_instr_tmp = act_instr + rotation;
      if( act_instr_tmp != act_instr ){
        if( act_instr_tmp<0 ){
          act_instr_tmp = 16; 
        }
        if( act_instr_tmp>16 ){
          act_instr_tmp = 0; // ACC
        }
        // store old Instrument-Beats
        // TODO
        sequencer_new_instr( act_instr_tmp );
      }
      break;
   
    case 1:
      // Select new Instrument
      act_instr_tmp =   act_instr  + rotation;
      if( act_instr_tmp != act_instr ){
        if( act_instr_tmp<0 ){
          act_instr_tmp = 16; 
        }
        if( act_instr_tmp>16 ){
          act_instr_tmp = 0; // ACC
        }
        // store old Instrument-Beats
        // TODO
        sequencer_new_instr( act_instr_tmp );
      }

      break;
      
    case 2: // Global Filter Effects and Pitch  
      tmp_x = 1 *2 + rotation;
      break;
      
    case 3: // Delay-Effect (Effects 2)  
      tmp_x = 1 *2 + rotation;
      break;

    case 4: // Soundset
       program_tmp =   program_tmp  + rotation;
       if( program_tmp >= countPrograms ){
         program_tmp = 0;
       }
       // program_tmp = map( program_midi,0,127,0,countPrograms-1 ); // 5 Sets );        
      break;

    case 5: // Tempo BPM
      // Change Speed by Buttons or Rotary
      old_bpm = bpm;
      tmp_bpm =  bpm *2 + rotation;
      bpm = 0.5f * tmp_bpm;
      if( bpm != old_bpm ){
        if( bpm < 30.0f ){
          bpm = 30.0f;
        }
        if( bpm > 285.0f ){
          bpm = 285.0f;
        }
        myTimer_Delta = sequencer_calc_delay( bpm );
      }
      break;
      
     default:
      tmp_x =   1 *2  + rotation;
      break;
  }  
}


void pcf_update_leds(){
  static unsigned long ledTimer=0;
  if (millis() - ledTimer > 50) {
    ledTimer = millis();
    // write to leds and invert current beat if playing
    // DEBUG("LEDS");
  }
}
  

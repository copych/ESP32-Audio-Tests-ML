# ESP32-Audio-Tests-ML
This is a twice modded repository of https://github.com/marcel-licence

The first mods are here https://github.com/ErichHeinemann/ESP32-Audio-Tests-ML

## ESP32-drum-comp
October 2022, a fork by Copych.
I liked the project by Erich Heinemann, but tried to use less peripherials.

The folder "ESP32-drum-comp" is a fork of "ESP32Core0_TEST_I2C_ssd1306_ADS11115_MIDI_PCF8574" project, now without PCFs and ADS. It uses 16ch multiplexer 74HC4067 for 16 buttons and 4 inputs of an internal ADC1 of ESP32 for pots. It also includes fully reworked button and encoder routines. Also I'm going to use addressable RGB-leds for indication purposes.


### Changelog
2022-11-06 Copych - There's no more need to store a pattern as 2 x 8 bit words, it's a uint16_t now

2022-10-25 Copych - 16 pattern buttons connected via multiplexer 74HC4067, and the rest are directly connected to GPIOs, so buttons processing is fully reworked

2022-10-20 Copych - Started a fork. Cut out most of the I2C code excepting the OLED, added DEBUG_ON definition


## History of a predicessor project by Erich Heinemann 
This Repository is a collection of modified Repositories originally created by Marcel Licence and modified by me in 2021.

Marcel Licence went forward and added a lot of features which my tests like the Volca beats Mod does not. The Volca is still at the version from early 2021!!

I had tested the Volca Beats MOD with several ESP32 Devekits and it works.

Mid 2022, fabien reportet, that this code does not compile with the Arduino IDE 2.0 RC ... I used the 1.8.19 in 2021.
It is up to You, Your risk, to use this code as an simplified version of marcels code.

The original Repositories are there, use these repos:
https://github.com/marcel-licence


### July 2021 - DIY Volca Percussion <b> Polca </b>
This project, which is a new extended version of the PCM-Mod is a standalone Volca-Thing, in my case it has the same size as my Volca Beats.
I am still in the stage of developing it, but a lot works already and makes a lot of fun.
https://github.com/ErichHeinemann/ESP32-Audio-Tests-ML/tree/main/ESP32Core0_TEST_I2C_ssd1306_ADS11115_MIDI_PCF8574


### April 2021 - Tests and Volca Beats PCM MOD
I did some testing with his code and it is really fine and sounds much better than expected, but he used a MIDI-Controller-Keyboard with specific CCs - and I don´t have one of these.

Today, April 2021, there are a lot of options to create a small Synthesizer or Drumcomputer based on Arduino / Teensy but the Projects from Marcel have the best Price/Sound - Ratio.

I tried to strip down his projects to make them a bit more unspecific and that way, it could be a bit more easy for others to start with his code.

Schematics and other documents will be provided later.

Video of the Volca Beats PCM MOD
https://www.youtube.com/watch?v=XIrn2-dZn1U

it is not really a mod, it is more a ADD. :)

/*
// Simple DIY Electronic Music Projects
//    diyelectromusic.wordpress.com
//
//  Arduino Mozzi Multi Pot MIDI FM Synthesis for MCP4725
//    https://diyelectromusic.wordpress.com/2020/09/29/mcp4725-and-mozzi-part-2/
//
      MIT License
      
      Copyright (c) 2020 diyelectromusic (Kevin)
      
      Permission is hereby granted, free of charge, to any person obtaining a copy of
      this software and associated documentation files (the "Software"), to deal in
      the Software without restriction, including without limitation the rights to
      use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
      the Software, and to permit persons to whom the Software is furnished to do so,
      subject to the following conditions:
      
      The above copyright notice and this permission notice shall be included in all
      copies or substantial portions of the Software.
      
      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
      IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
      FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
      COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHERIN
      AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
      WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/*
  Using principles from the following Arduino tutorials:
    Arduino MIDI Library  - https://github.com/FortySevenEffects/arduino_midi_library
    Mozzi Library         - https://sensorium.github.io/Mozzi/
    Arduino Potentiometer - https://www.arduino.cc/en/Tutorial/Potentiometer

  Inspired by an example from the Mozzi forums by user "Bayonet" and the
  Mozzi examples for FM synthesis.

*/
//#include <MIDI.h>

#include <MozziGuts.h>
#include <Oscil.h> // oscillator
#include <tables/cos2048_int8.h> // for the modulation oscillators
#include <tables/sin2048_int8.h> // sine table for oscillator
#include <tables/saw2048_int8.h> // saw table for oscillator
#include <tables/triangle2048_int8.h> // triangle table for oscillator
#include <tables/square_no_alias_2048_int8.h> // square table for oscillator
#include <mozzi_midi.h>
#include <Smooth.h>
#include <ADSR.h>
#include <mozzi_rand.h>

//Receiver code
#include <SoftwareSerial.h>
 
SoftwareSerial link(10, 11); // Rx, Tx

const int nPads = 12;
bool touched[nPads];

// Mozzi includes utility code for non-blocking TWI/I2C but it appears
// primarily for talking to I2C sensors at the CONTROL_RATE.
//
// This example winds that up to allow its use at AUDIO_RATE to output samples.
//
// This is the Mozzi-provided I2C handling.
#include <twi_nonblock.h>

#define MCP4725ADDR (0x60)

// Set the MIDI Channel to listen on
#define MIDI_CHANNEL 1

// Set up the analog inputs - comment out if you aren't using this one
//#define WAVT_PIN 0  // Wavetable
#define INTS_PIN 3  // FM intensity
// #define RATE_PIN 7  // Modulation Rate
//#define MODR_PIN 3  // Modulation Ratio
//#define AD_A_PIN 4  // ADSR Attack
//#define AD_D_PIN 5  // ADSR Delayhttps://github.com/sensorium/Mozzi.git
#define TEST_NOTE 50 // Comment out to test without MIDI
// #define DEBUG     1  // Comment out to remove debugging info - can only be used with TEST_NOTE
                       // Note: This will probably cause "clipping" of the audio...

#ifndef TEST_NOTE
struct MySettings : public MIDI_NAMESPACE::DefaultSettings {
  static const bool Use1ByteParsing = false; // Allow MIDI.read to handle all received data in one go
  static const long BaudRate = 31250;        // Doesn't build without this...
};
MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial, MIDI, MySettings);
#endif

#define CONTROL_RATE 256 // Hz, powers of 2 are most reliable
#define MIN_TRIG 1010
#define DEBOUNCE_DELAY 500 // ms

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

// Keeps track of the last pins touched
// so we know when buttons are 'released'
// uint16_t lasttouched = 0;
// uint16_t currtouched = 0;

// The original example used AutoMap to calibrate the range of values
// to be expected from the sensors. However AutoMap is really for use
// when you don't know the range of values that a sensor might produce,
// for example with a light dependant resistor.
//
// When you know the full range, e.g. when using a potentiometer, then
// AutoMap is largely obsolete.  And in my case, when there are some options
// to use a fixed value rather than arange then AutoMap is actually
// determinental to the final output.
//
// Experimentally I was finding AutoMap produced a very different output
// for a fixed value for the Intensity of 500 compared to a potentiometer
// reading a value of 500.  I still don't really know why, but I've taken
// out the AutoMap as a consequence and simplified the processing in the
// updateControl function too.
//
//AutoMap kMapIntensity(0,1023,10,700);
//AutoMap kMapModSpeed(0,1023,10,10000);

Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier;  // Wavetable will be set later
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModulator(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kIntensityMod(COS2048_DATA);

int wavetable;
int mod_ratio;
int carrier_freq;
long fm_intensity;
int adsr_a, adsr_d;
int testcount;
int lastL;

// smoothing for intensity to remove clicks on transitions
float smoothness = 0.95f;
Smooth <long> aSmoothIntensity(smoothness);
 
int potcount;
int potWAVT, potMODR, potINTS, potRATE, potAD_A, potAD_D;

// envelope generator
ADSR <CONTROL_RATE, AUDIO_RATE> envelope;

#define LED LED_BUILTIN // shows if MIDI is being recieved

const int seqLen = 16; // 16 step sequencer
const int DEFAULT_DURATION = 200;

uint8_t lastPitch = 0;
uint8_t baseNote = 60; // middle C
uint8_t sequence[seqLen] = { 
  60, 63, 65, 67,
  70, 72, 75, 77,
  79, 82, 84, 87,
  89, 91, 94, 96
};
int durations[seqLen] = {
  DEFAULT_DURATION, DEFAULT_DURATION, DEFAULT_DURATION, DEFAULT_DURATION,
  DEFAULT_DURATION, DEFAULT_DURATION, DEFAULT_DURATION, DEFAULT_DURATION,
  DEFAULT_DURATION, DEFAULT_DURATION, DEFAULT_DURATION, DEFAULT_DURATION,
  DEFAULT_DURATION, DEFAULT_DURATION, DEFAULT_DURATION, DEFAULT_DURATION
};
int seqPos = 0;
unsigned long seqNoteOff = 0;

// maps unpredictable inputs to a range
int automap(int reading, int minOut, int maxOut, int defaultOut) {
  static int minReading = 1024;
  static int maxReading = 0;

  minReading = min(minReading, reading);
  maxReading = max(maxReading, reading);
  if (minReading == maxReading) {
    return defaultOut;
  }
  return map(reading, minReading, maxReading, minOut, maxOut);
}

// output the note for the given sequence button index
void writeBtnPitch(uint8_t note) {
#ifdef DEBUG
    // Serial.print("< ");
    // Serial.print(btn);
    // Serial.print(": ");
//    Serial.println(note);
#endif
    lastPitch = note;
    HandleNoteOn (1, lastPitch, 127);
}

// turn off any currently-playing note
void shutUp() {
    HandleNoteOff(1, lastPitch, 0);
    lastPitch = 0; 
}

// increment through the sequence, looping to the front if at the end
void nextStep() {
  seqPos = (seqPos + 1) % seqLen;
  // Serial.println(seqPos);
}

void dacSetup (){
  // Set A2 and A3 as Outputs to make them our GND and Vcc,
  // which will power the MCP4725
//  pinMode(A2, OUTPUT);
//  pinMode(A3, OUTPUT);
//  digitalWrite(A2, LOW);//Set A2 as GND
//  digitalWrite(A3, HIGH);//Set A3 as Vcc

  // activate internal pullups for I2C
  digitalWrite(SDA, 1);
  digitalWrite(SCL, 1);

  initialize_twi_nonblock();

  // Update the TWI registers directly for fast mode I2C.
  // They will have already been preset to 100000 in twi_nonblock.cpp
  TWBR = ((F_CPU / 400000L) - 16) / 2;
}

void dacWrite (uint16_t value) {
  // There are several modes of writing DAC values (see the MCP4725 datasheet).
  // In summary:
  //     Fast Write Mode requires two bytes:
  //          0x0n + Upper 4 bits of data - d11-d10-d9-d8
  //                 Lower 8 bits of data - d7-d6-d5-d4-d3-d2-d1-d0
  //
  //     "Normal" DAC write requires three bytes:
  //          0x40 - Write DAC register (can use 0x50 if wanting to write to the EEPROM too)
  //          Upper 8 bits - d11-d10-d9-d9-d7-d6-d5-d4
  //          Lower 4 bits - d3-d2-d1-d0-0-0-0-0
  //
  uint8_t val1 = (value & 0xf00) >> 8; // Will leave top 4 bits zero = "fast write" command
  uint8_t val2 = (value & 0xff);
  twowire_beginTransmission(MCP4725ADDR);
  twowire_send (val1);
  twowire_send (val2);
  twowire_endTransmission();
}

void HandleNoteOn(byte channel, byte note, byte velocity) {
   if (velocity == 0) {
      HandleNoteOff(channel, note, velocity);
      return;
  }
  envelope.noteOff(); // Stop any already playing note
  carrier_freq = mtof(note);
  setFreqs();
  envelope.noteOn();
  digitalWrite(LED, HIGH);
}

void HandleNoteOff(byte channel, byte note, byte velocity) {
  if (carrier_freq == mtof(note)) {
    // If we are still playing the same note, turn it off
    envelope.noteOff();
  }

  digitalWrite(LED, LOW);
}

void playSequenceStep(int step) {
  const bool isRest = sequence[step] == -1;
  if (isRest) {
    shutUp();
  } else {
    writeBtnPitch(sequence[step]);
  }
}

void setup(){
  pinMode(LED, OUTPUT);
  link.begin(9600); // setup software serial

  for (int i = 0; i < nPads; i++) {
    touched[i] = false;
  }

  // --- Set up the MCP4725 board and the I2C library
  // WARNING: By default this is using A2-A5 for the MCP4725
  //          so these can't be used to control the synth...
  dacSetup();
  // --- End of the MCP4725 and I2C setup

#ifdef TEST_NOTE
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("Sequencer init...");
#endif
#else
  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  MIDI.setHandleNoteOn(HandleNoteOn);  // Put only the name of the function
  MIDI.setHandleNoteOff(HandleNoteOff);  // Put only the name of the function 
  MIDI.begin(MIDI_CHANNEL);
#endif

  adsr_a = 0;
  adsr_d = 150;
  setEnvelope(500);

  wavetable = 0;
  setWavetable();

  // Set default parameters for any potentially unused/unread pots
  potcount = 0;
  potWAVT = 2;
  potMODR = 5;
  potINTS = 500;
  potRATE = 150;
  potAD_A = 50;
  potAD_D = 200;

  startMozzi(CONTROL_RATE);
}

void setEnvelope(unsigned int sustain) {
  envelope.setADLevels(255, 64);
  envelope.setTimes(adsr_a, adsr_d, max(0, sustain - (200 + adsr_a + adsr_d)), 200); // 10000 is so the note will sustain 10 seconds unless a noteOff comes
}

void setFreqs(){
  //calculate the modulation frequency to stay in ratio
  int mod_freq = carrier_freq * mod_ratio;

  // set the FM oscillator frequencies
  aCarrier.setFreq(carrier_freq);
  aModulator.setFreq(mod_freq);
}

void setWavetable() {
  switch (wavetable) {
  case 1:
    aCarrier.setTable(TRIANGLE2048_DATA);
    break;
  case 2:
    aCarrier.setTable(SAW2048_DATA);
    break;
  case 3:
    aCarrier.setTable(SQUARE_NO_ALIAS_2048_DATA);
    break;
  default: // case 0
    aCarrier.setTable(SIN2048_DATA);
  }
}

unsigned int parseCharToHex(const char charX)
{
    if ('0' <= charX && charX <= '9') return charX - '0';
    if ('a' <= charX && charX <= 'f') return 10 + charX - 'a';
    if ('A' <= charX && charX <= 'F') return 10 + charX - 'A';
}

void readSerial() {
  const byte nChars = 32;
  static char cString[nChars];
  byte chPos = 0;
  byte ch = '\0';

  if (link.available() < 3) {
    return;
  }
  
  while (link.available() && chPos < nChars - 1 && ch != 'x') {
    // Serial.println(link.available());
    // read incoming char by char:
    const byte ch = link.read();
    // Serial.print(ch);
    cString[chPos] = ch;
    chPos++;
  }
  if (chPos > 0) {
    cString[chPos] = '\0';
    Serial.println(cString);

    const int touchedButton = parseCharToHex(cString[0]);
    Serial.println(touchedButton);
//    Serial.println(cString[1]);
    const bool isTouched = (cString[1] == 't');
    Serial.println(isTouched);
    touched[touchedButton] = isTouched;
    if (isTouched) {
      digitalWrite(LED, HIGH);
      writeBtnPitch(sequence[touchedButton]);
    } else {
      digitalWrite(LED, LOW);
      shutUp();
    }
  }
}

void updateControl() {
#ifndef TEST_NOTE
  MIDI.read();
#endif
    static unsigned long lastTriggerMs = 0;
    static int lastRead = 0;
    static const int trigPin = A1;
    unsigned long now = millis();

  // Read the potentiometers - do one on each updateControl scan.
  // Note: each potXXXX value is remembered between scans.
  potcount ++;
  if (potcount >= 6) potcount = 0;
  switch (potcount) {
  case 0:
#ifdef WAVT_PIN
    potWAVT = mozziAnalogRead(WAVT_PIN) >> 8; // value is 0-3
#endif
    break;
  case 1:
#ifdef INTS_PIN
    potINTS = automap(mozziAnalogRead(INTS_PIN), 0, 1023, potINTS); // value is 0-1023
#endif
    break;
  case 2:
#ifdef RATE_PIN
    potRATE = automap(mozziAnalogRead(RATE_PIN), 0, 1023, potRATE); // value is 0-1023
#endif
    break;
  case 3:
#ifdef MODR_PIN
    potMODR = mozziAnalogRead(MODR_PIN) >> 7; // value is 0-7
#endif
    break;
  case 4:
#ifdef AD_A_PIN
    potAD_A = mozziAnalogRead(AD_A_PIN) >> 3; // value is 0-255
#endif
    break;
  case 5:
#ifdef AD_D_PIN
    potAD_D = mozziAnalogRead(AD_D_PIN) >> 3; // value is 0-255
#endif
    break;
  default:
    potcount = 0;
  }

#ifdef TEST_NOTE
#ifdef DEBUG
//  Serial.print(potWAVT); Serial.print("\t");
//  Serial.print(potINTS); Serial.print("\t");
//  Serial.print(potRATE); Serial.print("\t");
//  Serial.print(potMODR); Serial.print("\t");
//  Serial.print(potAD_A); Serial.print("\t");
//  Serial.print(potAD_D); Serial.print("\t");
#endif
#endif

  // See if the wavetable changed...
  if (potWAVT != wavetable) {
    // Change the wavetable
    wavetable = potWAVT;
    setWavetable();
  }

  // See if the envelope changed...
  if ((potAD_A != adsr_a) || (potAD_D != adsr_d)) {
    // Change the envelope
    
    adsr_a = potAD_A;
    adsr_d = potAD_D;
    setEnvelope(lastRead > 0 ? (lastTriggerMs - now) : 10000);
  }

  // Everything else we update every cycle anyway
  mod_ratio = potMODR;

  // Perform the regular "control" updates
  envelope.update();
  setFreqs();

 // calculate the fm_intensity
  fm_intensity = ((long)potINTS * (kIntensityMod.next()+128))>>8; // shift back to range after 8 bit multiply

  // use a float here for low frequencies
  float mod_speed = (float)potRATE/100;
  kIntensityMod.setFreq(mod_speed);

  readSerial();
}


int updateAudio(){
  long modulation = aSmoothIntensity.next(fm_intensity) * aModulator.next();
//  return (int)((envelope.next() * aCarrier.phMod(modulation)) >> 8);

  // Use a value keeping 12 bits of resolution rather than scaling back to 8 bits
  // as per the original line above. This means we need to output a value between
  // 0 and 2047 from the original +/- 243 values Mozzi would normally expect
  // to be multiplying by the envelope here.
  //
  // NB: >>4 would give us 12 bits if the range was +/- 255, so to support
  //     values of +/- 243, perform an extra division by 2, hence >>5.
  //
  // Also need to bias the signal so that it will go from 0 to 2048 rather
  // then +/-1024.
  //
  int mozziVal = (int)(envelope.next() * aCarrier.phMod(modulation));
  
#ifdef DEBUG
//  Serial.print(">> \t");
//  Serial.print(mozziVal);
#endif

  uint16_t dac = map(mozziVal, -32768, 32768, 0, 4096); // mozziVal >> 5;
  dacWrite(dac);
#ifdef DEBUG
  lastL = dac;
//  Serial.print("\t>>\t");
//  Serial.println(dac);
#endif
  return 0;
}


void loop(){
  audioHook();
}

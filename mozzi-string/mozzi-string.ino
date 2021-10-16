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
#include <mozzi_midi.h> // for mtof
#include <Oscil.h>      // oscillator
#include <ADSR.h>       // envelope
// #include <LowPassFilter.h>
// #include <tables/cos2048_int8.h> // for the modulation oscillators
#include <tables/saw2048_int8.h> // saw table for oscillator
#include <tables/square_no_alias_2048_int8.h> // square table for oscillator
#include <tables/sin2048_int8.h> // sine table for oscillator
#include <tables/triangle2048_int8.h> // triangle table for oscillator
#include <tables/triangle_dist_cubed_2048_int8.h> // analog tri table for oscillator
#include <tables/triangle_dist_squared_2048_int8.h> // analog tri table for oscillator
#include <tables/triangle_hermes_2048_int8.h> // analog tri table for oscillator
#include <tables/triangle_valve_2048_int8.h> // analog tri table for oscillator
#include <tables/triangle_valve_2_2048_int8.h> // analog tri table for oscillator
//#include <Smooth.h>
// #include <mozzi_rand.h>

#include <InputDebounce.h>
#include <EEPROMex.h>
#include <EEPROMVar.h>

//Receiver code
#include <SoftwareSerial.h>

// Mozzi includes utility code for non-blocking TWI/I2C but it appears
// primarily for talking to I2C sensors at the CONTROL_RATE.
//
// This example winds that up to allow its use at AUDIO_RATE to output samples.
//
// This is the Mozzi-provided I2C handling.
#include <twi_nonblock.h>

#define MCP4725ADDR (0x60)

#define POTS 2

const int WAVS = 9;
#define WAVETABLE 0  // 0 = Saw; 1 = Square; 2 = Sine; 3+ = Triangle

// Set the MIDI Channel to listen on
#define MIDI_CHANNEL 1

// Set up the analog inputs - comment out if you aren't using this one
#define HARMONICS_PIN 0
#define GAIN_PIN 1
//#define WAVT_PIN 0  // Wavetable
// #define INTS_PIN 3  // FM intensity
// #define RATE_PIN 7  // Modulation Rate
//#define MODR_PIN 3  // Modulation Ratio
//#define AD_A_PIN 4  // ADSR Attack
//#define AD_D_PIN 5  // ADSR Delayhttps://github.com/sensorium/Mozzi.git
#define TEST_NOTE 50 // Comment out to test without MIDI
//#define DEBUG     1  // Comment out to remove debugging info - can only be used with TEST_NOTE
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
#define DEBOUNCE_DELAY 50 // ms

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

const int ANALOG_MAX = 1023;

//Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier;  // Wavetable will be set later
//Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModulator(COS2048_DATA);
//Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kIntensityMod(COS2048_DATA);
// Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kFilterMod(COS2048_DATA);
#define HARMONICS 6
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> oscs[HARMONICS + 1];  // Wavetables will be set later

// LowPassFilter lpf;

// Set up the multipliers
#define NUMWAVES 7
int waves[NUMWAVES][HARMONICS] = {
   {1, 1, 1, 1, 1, 1},
   {1, 3, 5, 7, 9, 11},
   {3, 5, 7, 9, 11, 13},
   {1, 2, 4, 6, 8, 10},
   {2, 4, 6, 8, 10, 12},
   {1, 2, 4, 8, 16, 32},
   {2, 4, 8, 16, 32, 64}
};

// Optional offset for each harmonic compared to carrier freq.
// Set to zero for "pure" harmonics.
const int DETUNES = 4;
int detune[DETUNES][HARMONICS] = {
  { 1,-1,2,-2,5,-5 }, 
  { 1,-1,3,-3,5,-5 }, 
  { 1,-1,2,-2,3,-3 }, 
  { 0,0,0,0,0,0 }
};

int harmonics; // index of the currently active harmonic set (vs the define, which is the number available)
long nextwave;
long lastwave;
 
const int gain = 255;  // [0] is the fixed gain for the carrier
int gainscale = 0;
int gainDelta = 0;
int wavetable = WAVETABLE;
int mod_ratio;
int carrier_freq;
//long fm_intensity;
int lastL;

// smoothing for intensity to remove clicks on transitions
//float smoothness = 0.95f;
//Smooth <long> aSmoothIntensity(smoothness);
 
// int potWAVT, potMODR, potINTS, potRATE, potAD_A, potAD_D;

// envelope generator
ADSR <CONTROL_RATE, AUDIO_RATE> envelope;
// ADSR <CONTROL_RATE, CONTROL_RATE> filterEnvelope;
const int ATTACK = 50;
const int DECAY = 200;
const int SUSTAIN_TIME = 10000;  // 10000 is so the note will sustain 10 seconds unless a noteOff comes
const int SUSTAIN_LEVEL_DECAY = 128;  // 10000 is so the note will sustain 10 seconds unless a noteOff comes
const int SUSTAIN_LEVEL_ORGAN = 255;  // 10000 is so the note will sustain 10 seconds unless a noteOff comes
const int RELEASE = 300;
int adsr_a, adsr_d, adsr_s, adsr_r;
// byte cutoff_freq = 255;  // filter range (0-255) corresponds with 0-8191Hz
// uint8_t resonance = 10; // range 0-255, 255 is most resonant

const int ENVELOPES = 3;
int envelopeIndex = 0;
int envelopes[ENVELOPES][4] = {
  { ATTACK, DECAY, SUSTAIN_LEVEL_DECAY, RELEASE }, 
  { 0, DECAY, SUSTAIN_LEVEL_DECAY, RELEASE }, 
  { 0, 0, SUSTAIN_LEVEL_ORGAN, 0 }
}; 

#define LED LED_BUILTIN // shows if MIDI is being recieved

SoftwareSerial link(10, 11); // Rx, Tx

const int nPads = 12;
bool touched[nPads];
uint8_t sequence[nPads] = { 
  48, 51, 55, 58,
  60, 63, 65, 67,
  70, 72, 75, 77//,
  // 79, 82, 84, 87
};
uint8_t lastPitch = 0;

// Buttons & memory
const int WAV_BTN = 0;
const int ENV_BTN = 1;
const int DETUNE_BTN = 2;
const int btnCount = 3; // Cycle waveform, select envelope, detune
const int pinBtn[16] = { 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0 }; // map pins to buttons
const int btnPin[btnCount] = { 4, 6, 8 }; // map buttons to pins
static InputDebounce buttons[btnCount];
static uint8_t btnValues[btnCount] = { 0, 0, 0 };
static uint8_t btnStates[btnCount] = { WAVS, ENVELOPES, DETUNES };

void readEEPROM() {
  EEPROM.readBlock<uint8_t>(0, btnValues, btnCount);
}

void updateEEPROM() {
  EEPROM.updateBlock<uint8_t>(0, btnValues, btnCount);
}

// maps unpredictable inputs to a range
int automap(int reading, int minOut, int maxOut, int defaultOut) {
  static int minReading = 4096;
  static int maxReading = -minReading;

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
    HandleNoteOn (1, lastPitch, 200);
}

// turn off any currently-playing note
void shutUp() {
    HandleNoteOff(1, lastPitch, 0);
    lastPitch = 0; 
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
  HandleNoteOff(channel, note, velocity); // Stop any already playing note
  if (velocity == 0) {
      return;
  }
  carrier_freq = mtof(note); 
  setFreqs();
  envelope.noteOn();
  // filterEnvelope.noteOn();
  digitalWrite(LED, HIGH);
}

void HandleNoteOff(byte channel, byte note, byte velocity) {
  // if (carrier_freq == mtof(note)) {
    // If we are still playing the same note, turn it off
    envelope.noteOff();
    // filterEnvelope.noteOff();
    carrier_freq = 0;
  // }

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

  adsr_a = ATTACK;//50;
  adsr_d = DECAY;//200;
  adsr_r = RELEASE;
  // lpf.setCutoffFreqAndResonance(cutoff_freq, resonance);
  setEnvelope(SUSTAIN_TIME);

  wavetable = WAVETABLE;
  setWavetable();

  // Set default parameters for any potentially unused/unread pots
  // potcount = 0;
  // potWAVT = 2;
  // potMODR = 5;
  // potINTS = 500;
  // potRATE = 150;
  // potAD_A = 50;
  // potAD_D = 200;
  rescaleGains();

  for (int i = 0; i < btnCount; i++) {
    // register callback functions (shared, used by all buttons)
    buttons[i].registerCallbacks(btn_pressedCallback, btn_releasedCallback, btn_pressedDurationCallback, btn_releasedDurationCallback);
    
    // setup input buttons (debounced)
    buttons[i].setup(btnPin[i], DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES); 
  }

  startMozzi(CONTROL_RATE);
}

void setEnvelope(unsigned int sustain) {
  envelope.setADLevels(255, adsr_s);
//  envelope.setTimes(adsr_a, adsr_d, max(0, sustain - (200 + adsr_a + adsr_d)), 200);
  envelope.setTimes(adsr_a, adsr_d, sustain, adsr_r);

  // filterEnvelope.setADLevels(255, adsr_s);
//  envelope.setTimes(adsr_a, adsr_d, max(0, sustain - (200 + adsr_a + adsr_d)), 200);
  // filterEnvelope.setTimes(adsr_a, adsr_d, sustain, adsr_r);
}

void setFreqs(){
  //calculate the modulation frequency to stay in ratio
//  int mod_freq = carrier_freq * mod_ratio;
//
//  // set the FM oscillator frequencies
//  aCarrier.setFreq(carrier_freq);
//  aModulator.setFreq(mod_freq);

  oscs[0].setFreq(carrier_freq);
  for (int i = 0; i < HARMONICS; i++) {
    oscs[i + 1].setFreq((carrier_freq+detune[btnValues[DETUNE_BTN]][i])*waves[harmonics][i]);
  }
}

void setTables(const int8_t * table) {
  for (int i = 0; i <= HARMONICS; i++) {
    oscs[i].setTable(table);
  }
}

void setWavetable() {
//  switch (wavetable) {
//  case 1:
//    aCarrier.setTable(TRIANGLE2048_DATA);
//    break;
//  case 2:
//    aCarrier.setTable(SAW2048_DATA);
//    break;
//  case 3:
//    aCarrier.setTable(SQUARE_NO_ALIAS_2048_DATA);
//    break;
//  default: // case 0
//    aCarrier.setTable(SIN2048_DATA);
//  }

  switch (wavetable) {
  case 1:
    setTables(SQUARE_NO_ALIAS_2048_DATA);
    break;
  case 2:
    setTables(SIN2048_DATA);
    break;
  case 3:
    setTables(TRIANGLE2048_DATA);
    break;
  case 4:
    setTables(TRIANGLE_DIST_CUBED_2048_DATA);
    break;
  case 5:
    setTables(TRIANGLE_DIST_SQUARED_2048_DATA);
    break;
  case 6:
    setTables(TRIANGLE_HERMES_2048_DATA);
    break;
  case 7:
    setTables(TRIANGLE_VALVE_2048_DATA);
    break;
  case 8:
    setTables(TRIANGLE_VALVE_2_2048_DATA);
    break;
  default: // case 0
    setTables(SAW2048_DATA);
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
#ifdef DEBUG
    Serial.println(cString);
#endif
    const int touchedButton = parseCharToHex(cString[0]);
//    Serial.println(touchedButton);
//    Serial.println(cString[1]);
    const bool isTouched = (cString[1] == 't');
//    Serial.println(isTouched);
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

void rescaleGains() {
  // Pre-estimate the eventual scaling factor required based on the
  // combined gain by counting the number of divide by 2s required to
  // get the total back under 256.  This is used in updateAudio for scaling.
  gainscale = 0;
  long gainval = 0;
  for (int i = 0; i <= HARMONICS; i++) {
    gainval += gain - (i * gainDelta);
  }
  gainval = 255 * gainval;
  while (gainval > 255) {
    gainscale++;
    gainval >>= 1;
  }
}

void updateControl() {
#ifdef USE_MIDI
  MIDI.read();
#endif
  unsigned long now = millis();
  static int gainReduction = 0;

  readSerial();
  for (int i = 0; i < btnCount; i++) {
    buttons[i].process(now);
  }

  // Read the potentiometers - do one on each updateControl scan.
  // static int potcount = 0;
  // if (potcount++ >= POTS) potcount = 0;

  // if (potcount == 0) {
#ifdef POT0
    int newharms = POT0;
#else
    int newharms = map(mozziAnalogRead(HARMONICS_PIN), 0, ANALOG_MAX, 0, NUMWAVES);  // Range 0 to 7
#endif
    if (newharms >= NUMWAVES) {
      newharms = NUMWAVES-1;
    }
    if (newharms != harmonics) {
      harmonics = newharms;
#ifdef DEBUG
      Serial.print("harmonics: ");
      Serial.println(harmonics);
#endif
      setFreqs();
    }
  // } else if (potcount == 1) {
    static const int MAX_GAIN_REDUCTION = 255 / HARMONICS;
    const int reading = map(mozziAnalogRead(GAIN_PIN), 0, ANALOG_MAX, 0, MAX_GAIN_REDUCTION);
    if (reading != gainDelta) {
      gainDelta = reading;
      rescaleGains();
#ifdef DEBUG
      Serial.print("gainDelta: ");
      Serial.print(gainDelta);
      Serial.print(" / ");
      Serial.println(MAX_GAIN_REDUCTION);
#endif
    }
//  }

// #ifdef DEBUG
//   Serial.print (harmonics);
//   Serial.print ("\t");
//   Serial.print (gainscale);
//   Serial.print ("\t");
//   Serial.print (lastwave);
//   Serial.print ("\t");
//   Serial.println (nextwave);
// #endif

  // See if the wavetable changed...
  if (btnValues[WAV_BTN] != wavetable) {
    // Change the wavetable
    wavetable = btnValues[WAV_BTN];
    setWavetable();
  }

  if (envelopeIndex != btnValues[ENV_BTN]) {
    envelopeIndex = btnValues[ENV_BTN];
    adsr_a = envelopes[envelopeIndex][0];
    adsr_d = envelopes[envelopeIndex][1];
    adsr_s = envelopes[envelopeIndex][2];
    adsr_r = envelopes[envelopeIndex][3];
    setEnvelope(SUSTAIN_TIME);
    // lpf.setCutoffFreqAndResonance(cutoff_freq, resonance);
  }
  
  // Perform the regular "control" updates
  envelope.update();
  // filterEnvelope.update();

  // map the modulation into the filter range (0-255), corresponds with 0-8191Hz
  // cutoff_freq = filterEnvelope.next();
}

int updateAudio() {
  lastwave = 0;
  for (int i = 0; i <= HARMONICS; i++) {
    lastwave += (long)(gain - gainDelta * i) * oscs[i].next();
  }
  nextwave  = //lpf.next
    ( lastwave >> gainscale );

  // long modulation = aSmoothIntensity.next(fm_intensity) * aModulator.next();
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
  // int mozziVal = (int)(envelope.next() * aCarrier.phMod(modulation));
  int mozziVal = (int)(envelope.next() * nextwave);
#ifdef DEBUG
//  static int maxMozziVal = 0;
//  maxMozziVal = max(maxMozziVal, mozziVal);
//  Serial.print(">> \t");
//  Serial.print(maxMozziVal);
#endif

  uint16_t dac = map(mozziVal, -32768, 32768, 0, 4096); // mozziVal >> 5;
  dacWrite(max(0, min(4096, dac)));
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

void btn_pressedCallback(uint8_t pinIn)
{
  // handle pressed state
   digitalWrite(LED, HIGH); // turn the LED on
#ifdef DEBUG
   Serial.print("BUTTON (pin: ");
   Serial.print(pinIn);
   Serial.println(")");
#endif
}

void btn_releasedCallback(uint8_t pinIn)
{
  // handle released state
  digitalWrite(LED, LOW); // turn the LED on
  const int btnIndex = pinBtn[pinIn];
  btnValues[btnIndex] = (btnValues[btnIndex] + 1) % btnStates[btnIndex];
}

void btn_pressedDurationCallback(uint8_t pinIn, unsigned long duration)
{
  // handle still pressed state
  // Serial.print("SEQ HIGH (pin: ");
  // Serial.print(pinIn);
  // Serial.print(") still pressed, duration ");
  // Serial.print(duration);
  // Serial.println("ms");
  // writeBtnPitch(pinBtn[pinIn] - ctrlBtnCount);
}

void btn_releasedDurationCallback(uint8_t pinIn, unsigned long duration)
{
  // handle released state
  // Serial.print("SEQ LOW (pin: ");
  // Serial.print(pinIn);
  // Serial.print("), duration ");
  // Serial.print(duration);
  // Serial.println("ms");
}

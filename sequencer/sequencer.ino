/**************************************************************************/
/*!
    @file     sequencer.ino
    @author   Peter MacMurchy
    @license  see license.txt

    This sketch will record a sequence from the LittleBits keyboard and play it back

    This is based on an example sketch for the Adafruit MCP4725 breakout board
    ----> http://www.adafruit.com/products/935

    Inputs: 
    A0: Trigger bitsnap
    A1: Note bitsnap
    D8: Button (toggle play/rec)

    Ouputs:
    MCP4725
*/
/**************************************************************************/
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <InputDebounce.h>
#include <EEPROMex.h>
#include <EEPROMVar.h>

Adafruit_MCP4725 dac;

const int playPin = 10;
const int seqBtnCount = 4;
const int ctrlBtnCount = 1; // Just the play/rec button for now
const int seqPin[seqBtnCount] = { 4, 6, 8, 9 };
const int pinBtn[16] = { 0, 0, 0, 0, 1, 0, 2, 0, 3, 4, 0, 0, 0, 0, 0, 0 }; // map pins to buttons
const int ledPin = LED_BUILTIN;
const int trigPin = A0;
const int PLAY_MODE = 0;
const int REC_MODE = 1;
const int seqLen = 16; // 16 step sequencer
const int DEBOUNCE_DELAY = 150;
const int DEFAULT_DURATION = 250;

static InputDebounce buttons[seqBtnCount + ctrlBtnCount];

int mode = PLAY_MODE;
int lastPitch = 0;
int sequence[seqLen] = { 
  int(random(seqBtnCount)), int(random(seqBtnCount)), int(random(seqBtnCount)), int(random(seqBtnCount)),
  int(random(seqBtnCount)), int(random(seqBtnCount)), int(random(seqBtnCount)), int(random(seqBtnCount)),
  int(random(seqBtnCount)), int(random(seqBtnCount)), int(random(seqBtnCount)), int(random(seqBtnCount)),
  int(random(seqBtnCount)), int(random(seqBtnCount)), int(random(seqBtnCount)), int(random(seqBtnCount))
};
int durations[seqLen] = {
  int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)),
  int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)),
  int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)),
  int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION)), int(random(DEFAULT_DURATION))
};
int seqPos = 0;
unsigned long seqNoteOff = 0;

void readEEPROM() {
  EEPROM.readBlock<int>(0, sequence, seqLen);
  EEPROM.readBlock<int>(sizeof(sequence), durations, seqLen);
}

void updateEEPROM() {
  EEPROM.updateBlock<int>(0, sequence, seqLen);
  EEPROM.updateBlock<int>(sizeof(sequence), durations, seqLen);
}

// output the note for the given sequence button index
void writeBtnPitch(int btn) {
    static const uint16_t pitches[seqBtnCount] = { 512, 1024, 2048, 3072 };
    // Serial.print("< ");
    // Serial.print(btn);
    // Serial.print(": ");
    // Serial.println(pitches[btn]);
    dac.setVoltage(pitches[btn], false);
}

// turn off any currently-playing note
void shutUp() {
    dac.setVoltage(0, false);
}

void seqBtn_pressedCallback(uint8_t pinIn)
{
  // handle pressed state
  digitalWrite(ledPin, HIGH); // turn the LED on
  // Serial.print("SEQ HIGH (pin: ");
  // Serial.print(pinIn);
  // Serial.println(")");
  writeBtnPitch(pinBtn[pinIn] - ctrlBtnCount);
}

// increment through the sequence, looping to the front if at the end
void nextStep() {
  seqPos = (seqPos + 1) % seqLen;
  // Serial.println(seqPos);
}

void seqBtn_releasedCallback(uint8_t pinIn)
{
  // handle released state
  // Serial.print("SEQ LOW (pin: ");
  // Serial.print(pinIn);
  // Serial.println(")");
  if (mode == REC_MODE) {
    digitalWrite(ledPin, LOW); // turn the LED off
    sequence[seqPos] = pinBtn[pinIn] - ctrlBtnCount;
    nextStep();
    shutUp();
  }
}

void seqBtn_pressedDurationCallback(uint8_t pinIn, unsigned long duration)
{
  // handle still pressed state
  // Serial.print("SEQ HIGH (pin: ");
  // Serial.print(pinIn);
  // Serial.print(") still pressed, duration ");
  // Serial.print(duration);
  // Serial.println("ms");
  writeBtnPitch(pinBtn[pinIn] - ctrlBtnCount);
}

void seqBtn_releasedDurationCallback(uint8_t pinIn, unsigned long duration)
{
  // handle released state
  // Serial.print("SEQ LOW (pin: ");
  // Serial.print(pinIn);
  // Serial.print("), duration ");
  // Serial.print(duration);
  // Serial.println("ms");
  if (mode == REC_MODE) {
    durations[seqPos] = duration;
  }
}

void onTogglePlayRec() {
  mode = !mode;
  seqPos = 0;
  shutUp();

  if (mode == PLAY_MODE) {
    Serial.println("PLAY; Saving ");
    for (int i = 0; i < seqLen; i++) {
      Serial.print(sequence[i]);
      Serial.print(", ");
    }
    updateEEPROM();
  } else {
    Serial.println("REC");
  }
}

void playBtn_pressedCallback(uint8_t pinIn)
{
  // handle pressed state
  digitalWrite(ledPin, HIGH); // turn the LED on
  Serial.print("PLAY HIGH (pin: ");
  Serial.print(pinIn);
  Serial.println(")");
}

void playBtn_releasedCallback(uint8_t pinIn)
{
  // handle released state
  digitalWrite(ledPin, LOW); // turn the LED off
  Serial.print("PLAY LOW (pin: ");
  Serial.print(pinIn);
  Serial.println(")");
  onTogglePlayRec();
}

void playBtn_pressedDurationCallback(uint8_t pinIn, unsigned long duration)
{
  // handle still pressed state
  // Serial.print("PLAY HIGH (pin: ");
  // Serial.print(pinIn);
  // Serial.print(") still pressed, duration ");
  // Serial.print(duration);
  // Serial.println("ms");
}

void playBtn_releasedDurationCallback(uint8_t pinIn, unsigned long duration)
{
  // handle released state
  // Serial.print("PLAY LOW (pin: ");
  // Serial.print(pinIn);
  // Serial.print("), duration ");
  // Serial.print(duration);
  // Serial.println("ms");
}

void setup(void) {
  Serial.begin(9600);
  Serial.println("Sequencer init...");
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(ledPin, OUTPUT);

  buttons[0].registerCallbacks(playBtn_pressedCallback, playBtn_releasedCallback, playBtn_pressedDurationCallback, playBtn_releasedDurationCallback);
  buttons[0].setup(playPin, DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES, 300); // single-shot pressed-on time duration callback

  for (int i = 0; i < seqBtnCount; i++) {
    // register callback functions (shared, used by all buttons)
    int btnIdx = i + ctrlBtnCount;
    buttons[btnIdx].registerCallbacks(seqBtn_pressedCallback, seqBtn_releasedCallback, seqBtn_pressedDurationCallback, seqBtn_releasedDurationCallback);
    
    // setup input buttons (debounced)
    buttons[btnIdx].setup(seqPin[i], DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES); 
  }
  
  Serial.println("Reading EEPROM...");
  readEEPROM();

  // For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
  // For MCP4725A0 the address is 0x60 or 0x61
  // For MCP4725A2 the address is 0x64 or 0x65
  dac.begin(0x60); // I solder the 2 pins on the GND end together, setting it to 0x60
  shutUp();

  Serial.println("Let's bleep!");
}

void playSequenceStep(int step) {
  const bool isRest = sequence[step] == -1;
  if (isRest) {
    shutUp();
  } else {
    writeBtnPitch(sequence[step]);
  }
}

void loop(void) {
    static unsigned long lastTriggerMs = 0;
    unsigned long now = millis();
    static bool on = false;
    static int lastRead = 0;

    for (int i = 0, len = seqBtnCount + ctrlBtnCount; i < len; i++) {
      buttons[i].process(now);
    }

    if (mode == PLAY_MODE) {
      if (now <= seqNoteOff) {
        playSequenceStep(seqPos);
      } else {
        shutUp();
        digitalWrite(ledPin, LOW);
        on = !on;
      }

      // look for triggers and advance sequence when found
      if (now - lastTriggerMs > DEBOUNCE_DELAY && now > seqNoteOff) {
        // read the trigger on analog pin 0:
        int sensorValue = analogRead(trigPin);
        // Serial.print("> ");
        // Serial.println(sensorValue);
        // The analog reading goes from 0 - 1023
        if (sensorValue == 1023 && lastRead != sensorValue) {
          nextStep();
          lastTriggerMs = now;
          seqNoteOff = now + durations[seqPos];

          // toggle the LED each retrig
          digitalWrite(ledPin, on ? LOW : HIGH);
          on = !on;
        }
        lastRead = sensorValue;
      }
    }
}

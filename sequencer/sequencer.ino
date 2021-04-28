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
int mode = PLAY_MODE;
int lastPitch = 0;
int sequence[seqLen] = { 
  0, 1, 2, 3,
  -1, -1, -1, -1,
  -1, -1, -1, -1,
  -1, -1, -1, -1
};
int seqPos = 0;
static InputDebounce buttons[seqBtnCount + ctrlBtnCount];


void playBtn(int btn) {
    // set the note based on A1
    static const uint16_t pitches[seqBtnCount] = { 512, 1024, 2048, 3072 };
    // Serial.print("< ");
    // Serial.print(btn);
    // Serial.print(": ");
    // Serial.println(pitches[btn]);
    dac.setVoltage(pitches[btn], false);
}

void shutUp() {
    dac.setVoltage(0, false);
}

void seqBtn_pressedCallback(uint8_t pinIn)
{
  // handle pressed state
  digitalWrite(ledPin, HIGH); // turn the LED on
  Serial.print("SEQ HIGH (pin: ");
  Serial.print(pinIn);
  Serial.println(")");
  playBtn(pinBtn[pinIn] - ctrlBtnCount);
}

void nextStep() {
  seqPos = (seqPos + 1) % seqLen;
  Serial.println(seqPos);
}

void seqBtn_releasedCallback(uint8_t pinIn)
{
  // handle released state
  digitalWrite(ledPin, LOW); // turn the LED off
  Serial.print("SEQ LOW (pin: ");
  Serial.print(pinIn);
  Serial.println(")");
  sequence[seqPos] = pinBtn[pinIn] - ctrlBtnCount;
  nextStep();
  shutUp();
}

void seqBtn_pressedDurationCallback(uint8_t pinIn, unsigned long duration)
{
  // handle still pressed state
  // Serial.print("SEQ HIGH (pin: ");
  // Serial.print(pinIn);
  // Serial.print(") still pressed, duration ");
  // Serial.print(duration);
  // Serial.println("ms");
  playBtn(pinBtn[pinIn] - ctrlBtnCount);
}

void seqBtn_releasedDurationCallback(uint8_t pinIn, unsigned long duration)
{
  // handle released state
  // Serial.print("SEQ LOW (pin: ");
  // Serial.print(pinIn);
  // Serial.print("), duration ");
  // Serial.print(duration);
  // Serial.println("ms");
}

void onTogglePlayRec() {
  mode = !mode;
  seqPos = 0;
  shutUp();

  if (mode == PLAY_MODE) {
    for (int i = 0; i < seqLen; i++) {
      Serial.print(sequence[i]);
      Serial.print(", ");
    }
    Serial.println("PLAY");
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
  Serial.println("Sequencer init");
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  // pinMode(playPin, INPUT_PULLUP);
  buttons[0].registerCallbacks(playBtn_pressedCallback, playBtn_releasedCallback, playBtn_pressedDurationCallback, playBtn_releasedDurationCallback);
  buttons[0].setup(playPin, DEFAULT_INPUT_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES, 300); // single-shot pressed-on time duration callback

  for (int i = 0; i < seqBtnCount; i++) {
    // pinMode(seqPin[i], INPUT_PULLUP);
    // register callback functions (shared, used by all buttons)
    int btnIdx = i + ctrlBtnCount;
    buttons[btnIdx].registerCallbacks(seqBtn_pressedCallback, seqBtn_releasedCallback, seqBtn_pressedDurationCallback, seqBtn_releasedDurationCallback);
    
    // setup input buttons (debounced)
    buttons[btnIdx].setup(seqPin[i], DEFAULT_INPUT_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES); 
  }
  
  // For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
  // For MCP4725A0 the address is 0x60 or 0x61
  // For MCP4725A2 the address is 0x64 or 0x65
  dac.begin(0x60);
}

// int lerp(uint16_t a, uint16_t b, uint16_t i, uint16_t steps) {
//   float t = float(i) / float(steps);
//   float delta = float(b) - float(a);
//   uint16_t result = round(a + t * delta);
//   return result;
// }


void checkPlayButton(unsigned long now) {
    buttons[0].process(now);
}

// REC mode
void checkSeqButton(int i, unsigned long now) {
  buttons[i].process(now);
}

void playSequenceStep(int step) {
  const bool isRest = sequence[step] == -1;
  digitalWrite(LED_BUILTIN, isRest ? LOW : HIGH);
  if (isRest) {
    shutUp();
  } else {
    playBtn(sequence[step]);
  }
}

void loop(void) {
    static unsigned long lastTriggerMs = 0;
    unsigned long now = millis();
    static bool on = false;
    static int lastRead = 0;

    checkPlayButton(now);

    if (mode == REC_MODE) {
      for (int i = 0; i < seqBtnCount; i++) {
        checkSeqButton(i + ctrlBtnCount, now);
      }
    } else {
      playSequenceStep(seqPos);

      // look for triggers and advance sequence when found
      if (now - lastTriggerMs > 150) {
        // read the trigger on analog pin 0:
        int sensorValue = analogRead(trigPin);
        // Serial.print("> ");
        // Serial.println(sensorValue);
        // The analog reading goes from 0 - 1023
        if (sensorValue == 1023 && lastRead != sensorValue) {
          nextStep();
          lastTriggerMs = now;

          // toggle the LED each retrig
          // digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
          on = !on;
        }
        lastRead = sensorValue;
      }
    }
    delay(1);
}

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

Adafruit_MCP4725 dac;

const int playPin = 10;
const int seqBtnCount = 4;
const int seqPin[seqBtnCount] = { 4, 6, 8, 9 };
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

void setup(void) {
  Serial.begin(9600);
  Serial.println("Sequencer init");
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(playPin, INPUT_PULLUP);
  for (int i = 0; i < seqBtnCount; i++) {
    pinMode(seqPin[i], INPUT_PULLUP);
  }
  
  // For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
  // For MCP4725A0 the address is 0x60 or 0x61
  // For MCP4725A2 the address is 0x64 or 0x65
  dac.begin(0x60);
}

int lerp(uint16_t a, uint16_t b, uint16_t i, uint16_t steps) {
  float t = float(i) / float(steps);
  float delta = float(b) - float(a);
  uint16_t result = round(a + t * delta);
  return result;
}

void onTogglePlayRec() {
  mode = !mode;
  seqPos = 0;
  shutUp();

  if (mode == PLAY_MODE) {
    Serial.println("PLAY");
  } else {
    Serial.println("REC");
  }
}

void playBtn(int btn) {
    // set the note based on A1
    static const uint16_t pitches[seqBtnCount] = { 512, 1024, 2048, 3072 };
    Serial.print("< ");
    Serial.print(btn);
    Serial.print(": ");
    Serial.println(pitches[btn]);
    dac.setVoltage(pitches[btn], false);
}

void shutUp() {
    dac.setVoltage(0, false);
}

void checkPlayButton() {
    static int playState = 0;
    static int lastPlayState = 0;

    // read the state of the pushbutton value:
    playState = digitalRead(playPin);
    if (lastPlayState != playState) {
      digitalWrite(ledPin, !playState);
      if (playState == LOW) {
        onTogglePlayRec();
      }
    }
    lastPlayState = playState;
}

void nextStep() {
  seqPos = (seqPos + 1) % seqLen;
  Serial.println(seqPos);
}

// REC mode
void checkSeqButton(int i) {
  static int buttonStates[seqBtnCount] = { HIGH, HIGH, HIGH, HIGH };
  static int buttonLastStates[seqBtnCount] = { HIGH, HIGH, HIGH, HIGH };

  buttonStates[i] = digitalRead(seqPin[i]);
  if (buttonStates[i] == LOW) {
    playBtn(i);
    if (buttonLastStates[i] == HIGH) {
      sequence[seqPos] = i;
      nextStep();
    }
  } else {
    shutUp();
  }
  buttonLastStates[i] = buttonStates[i];
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
    unsigned long nowMs = millis();
    static bool on = false;
    static int lastRead = 0;

    checkPlayButton();

    if (mode == REC_MODE) {
      for (int i = 0; i < seqBtnCount; i++) {
        checkSeqButton(i);
      }
    } else {
      playSequenceStep(seqPos);

      // look for triggers and advance sequence when found
      if (nowMs - lastTriggerMs > 150) {
        // read the trigger on analog pin 0:
        int sensorValue = analogRead(trigPin);
        Serial.print("> ");
        Serial.println(sensorValue);
        // The analog reading goes from 0 - 1023
        if (sensorValue == 1023 && lastRead != sensorValue) {
          nextStep();
          lastTriggerMs = nowMs;

          // toggle the LED each retrig
          digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
          on = !on;
        }
        lastRead = sensorValue;
      }
    }
}

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

void setup(void) {
  Serial.begin(9600);
  Serial.println("Sequencer init");
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  
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


const int buttonPin = 8;
const int ledPin = LED_BUILTIN;
const int trigPin = A1;
const int pitchPin = A0;
const int PLAY_MODE = 0;
const int REC_MODE = 1;
int mode = PLAY_MODE;
int lastPitch = 0;

void onButtonUp() {
  mode = !mode;
  if (mode == PLAY_MODE) {
    Serial.println("PLAY");
  } else {
    Serial.println("REC");
  }
}

void onTrig() {
    // set the note based on A1
    delay(30);
    int pitch = analogRead(pitchPin);
    lastPitch = map(pitch, 0, 1023, 0, 4095);
    Serial.print("Pitch: ");
    Serial.print(pitch);
    Serial.print(": ");
    Serial.println(lastPitch);
    dac.setVoltage(lastPitch, false);
}

void checkButton() {
    static int buttonState = 0;
    static int buttonLastState = 0;

    // read the state of the pushbutton value:
    buttonState = digitalRead(buttonPin);
  
    // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
    if (buttonState == HIGH) {
      // turn LED on:
      digitalWrite(ledPin, HIGH);
    } else {
      // turn LED off:
      digitalWrite(ledPin, LOW);
      if (buttonLastState != buttonState) {
        onButtonUp();
      }
    }
    buttonLastState = buttonState;
}

void loop(void) {
    static unsigned long lastTriggerMs = 0;
    unsigned long nowMs = millis();
    static bool on = false;
    static int lastRead = 0;

    checkButton();
    
    // look for triggers and reset to near the peak of the sine wave
    if (nowMs - lastTriggerMs > 150) {
      // read the trigger on analog pin 0:
      int sensorValue = analogRead(trigPin );
      // The analog reading goes from 0 - 1023
      if (sensorValue == 1023 && lastRead != sensorValue) {
        onTrig();
        lastTriggerMs = nowMs;

        // toggle the LED each retrig
        digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
        on = !on;
      }
      lastRead = sensorValue;
    }

}

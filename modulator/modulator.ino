/*!
 * 
    Inputs: 
    A0: interpSteps pot
    A1: Trigger bitsnap
    D4: Button (switch trig vs free)
    D6: Button (switch triangle vs sine)

    Ouputs:
    MCP4725
 */

// Adapted from:
/**************************************************************************/
/*!
    @file     sinewave.pde
    @author   Adafruit Industries
    @license  BSD (see license.txt)

    This example will generate a sine wave with the MCP4725 DAC.

    This is an example sketch for the Adafruit MCP4725 breakout board
    ----> http://www.adafruit.com/products/935

    Adafruit invests time and resources providing this open source code,
    please support Adafruit and open-source hardware by purchasing
    products from Adafruit!

*/
/**************************************************************************/
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <InputDebounce.h>
#include <EEPROMex.h>
#include <EEPROMVar.h>

Adafruit_MCP4725 dac;

const int ledPin = LED_BUILTIN;
const int ratePin = A0;
const int trigPin = A1;
const int defaultEnvSteps = 4;
const int maxEnvSteps = 32;
const int maxLfoSteps = 4095;
const int envStartStep = 131;
const int defaultLFOSteps = 120;
const int btnCount = 4; // env vs lfo, sine vs tri, rate (high vs low), use rate pin
const int btnPin[btnCount] = { 4, 6, 8, 9 };
const int pinBtn[10] = { 0, 0, 0, 0, 0, 1, 1, 1, 2, 3 };
const int DEBOUNCE_DELAY = 30;
const int MIN_TRIG = 1010; // least ammount considered a trigger (is ~1019 if a pulldown is connected)
const int MAX_TROUGH = 3; // largest amount considered a trough, indicating the trigger is off

static InputDebounce buttons[btnCount];
static bool toggles[btnCount] = { true, true, true, true };

/* Note: If flash space is tight a quarter sine wave is enough
   to generate full sine and cos waves, but some additional
   calculation will be required at each step after the first
   quarter wave.                                              */

const PROGMEM uint16_t DACLookup_FullSine_9Bit[512] =
{
  2048, 2073, 2098, 2123, 2148, 2174, 2199, 2224,
  2249, 2274, 2299, 2324, 2349, 2373, 2398, 2423, // 8
  2448, 2472, 2497, 2521, 2546, 2570, 2594, 2618, // 16
  2643, 2667, 2690, 2714, 2738, 2762, 2785, 2808, 
  2832, 2855, 2878, 2901, 2924, 2946, 2969, 2991, // 32
  3013, 3036, 3057, 3079, 3101, 3122, 3144, 3165,
  3186, 3207, 3227, 3248, 3268, 3288, 3308, 3328,
  3347, 3367, 3386, 3405, 3423, 3442, 3460, 3478,
  3496, 3514, 3531, 3548, 3565, 3582, 3599, 3615, // 64
  3631, 3647, 3663, 3678, 3693, 3708, 3722, 3737,
  3751, 3765, 3778, 3792, 3805, 3817, 3830, 3842,
  3854, 3866, 3877, 3888, 3899, 3910, 3920, 3930,
  3940, 3950, 3959, 3968, 3976, 3985, 3993, 4000,
  4008, 4015, 4022, 4028, 4035, 4041, 4046, 4052,
  4057, 4061, 4066, 4070, 4074, 4077, 4081, 4084,
  4086, 4088, 4090, 4092, 4094, 4095, 4095, 4095,
  4095, 4095, 4095, 4095, 4094, 4092, 4090, 4088, // 128
  4086, 4084, 4081, 4077, 4074, 4070, 4066, 4061,
  4057, 4052, 4046, 4041, 4035, 4028, 4022, 4015,
  4008, 4000, 3993, 3985, 3976, 3968, 3959, 3950,
  3940, 3930, 3920, 3910, 3899, 3888, 3877, 3866,
  3854, 3842, 3830, 3817, 3805, 3792, 3778, 3765,
  3751, 3737, 3722, 3708, 3693, 3678, 3663, 3647,
  3631, 3615, 3599, 3582, 3565, 3548, 3531, 3514,
  3496, 3478, 3460, 3442, 3423, 3405, 3386, 3367,
  3347, 3328, 3308, 3288, 3268, 3248, 3227, 3207,
  3186, 3165, 3144, 3122, 3101, 3079, 3057, 3036,
  3013, 2991, 2969, 2946, 2924, 2901, 2878, 2855,
  2832, 2808, 2785, 2762, 2738, 2714, 2690, 2667,
  2643, 2618, 2594, 2570, 2546, 2521, 2497, 2472,
  2448, 2423, 2398, 2373, 2349, 2324, 2299, 2274,
  2249, 2224, 2199, 2174, 2148, 2123, 2098, 2073,
  2048, 2023, 1998, 1973, 1948, 1922, 1897, 1872,
  1847, 1822, 1797, 1772, 1747, 1723, 1698, 1673,
  1648, 1624, 1599, 1575, 1550, 1526, 1502, 1478,
  1453, 1429, 1406, 1382, 1358, 1334, 1311, 1288,
  1264, 1241, 1218, 1195, 1172, 1150, 1127, 1105,
  1083, 1060, 1039, 1017,  995,  974,  952,  931,
   910,  889,  869,  848,  828,  808,  788,  768,
   749,  729,  710,  691,  673,  654,  636,  618,
   600,  582,  565,  548,  531,  514,  497,  481,
   465,  449,  433,  418,  403,  388,  374,  359,
   345,  331,  318,  304,  291,  279,  266,  254,
   242,  230,  219,  208,  197,  186,  176,  166,
   156,  146,  137,  128,  120,  111,  103,   96,
    88,   81,   74,   68,   61,   55,   50,   44,
    39,   35,   30,   26,   22,   19,   15,   12,
    10,    8,    6,    4,    2,    1,    1,    0,
     0,    0,    1,    1,    2,    4,    6,    8,
    10,   12,   15,   19,   22,   26,   30,   35,
    39,   44,   50,   55,   61,   68,   74,   81,
    88,   96,  103,  111,  120,  128,  137,  146,
   156,  166,  176,  186,  197,  208,  219,  230,
   242,  254,  266,  279,  291,  304,  318,  331,
   345,  359,  374,  388,  403,  418,  433,  449,
   465,  481,  497,  514,  531,  548,  565,  582,
   600,  618,  636,  654,  673,  691,  710,  729,
   749,  768,  788,  808,  828,  848,  869,  889,
   910,  931,  952,  974,  995, 1017, 1039, 1060,
  1083, 1105, 1127, 1150, 1172, 1195, 1218, 1241,
  1264, 1288, 1311, 1334, 1358, 1382, 1406, 1429,
  1453, 1478, 1502, 1526, 1550, 1575, 1599, 1624,
  1648, 1673, 1698, 1723, 1747, 1772, 1797, 1822,
  1847, 1872, 1897, 1922, 1948, 1973, 1998, 2023
};

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

int lerp(uint16_t a, uint16_t b, uint16_t i, uint16_t steps) {
  float t = float(i) / float(steps);
  float delta = float(b) - float(a);
  uint16_t result = round(a + t * delta);
  return result;
}

void readEEPROM() {
  EEPROM.readBlock<bool>(0, toggles, btnCount);
}

void updateEEPROM() {
  EEPROM.updateBlock<bool>(0, toggles, btnCount);
}

void btn_pressedCallback(uint8_t pinIn)
{
  // handle pressed state
  digitalWrite(ledPin, HIGH); // turn the LED on
  Serial.print("button HIGH (pin: ");
  Serial.print(pinIn);
  Serial.println(")");
}

void btn_releasedCallback(uint8_t pinIn)
{
  // handle released state
  digitalWrite(ledPin, LOW); // turn the LED off
  Serial.print("button LOW (pin: ");
  Serial.print(pinIn);
  Serial.println(")");
  toggles[pinBtn[pinIn]] = !toggles[pinBtn[pinIn]];
  updateEEPROM();
}

void btn_pressedDurationCallback(uint8_t pinIn, unsigned long duration)
{
  // handle still pressed state
  // Serial.print("PLAY HIGH (pin: ");
  // Serial.print(pinIn);
  // Serial.print(") still pressed, duration ");
  // Serial.print(duration);
  // Serial.println("ms");
}

void btn_releasedDurationCallback(uint8_t pinIn, unsigned long duration)
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
  Serial.println("Modulator init!");

  readEEPROM();

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  
  for (int i = 0; i < btnCount; i++) {
    // register callback functions (shared, used by all buttons)
    int btnIdx = i;
    buttons[btnIdx].registerCallbacks(btn_pressedCallback, btn_releasedCallback, btn_pressedDurationCallback, btn_releasedDurationCallback);
    
    // setup input buttons (debounced)
    buttons[btnIdx].setup(btnPin[i], DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES); 
  }

  // For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
  // For MCP4725A0 the address is 0x60 or 0x61
  // For MCP4725A2 the address is 0x64 or 0x65
  dac.begin(0x60);
}

void loop(void) {
    static uint16_t step = 0; // current table step
    static unsigned long lastTriggerMs = 0;
    unsigned long nowMs = millis();
    static bool on = false;
    static int lastRead = 0;
    const bool isTriggered = toggles[0];
    const bool isSine = toggles[1];
    const bool isLowRate = toggles[2];
    const bool useRatePin = toggles[3];

    for (int btn = 0; btn < btnCount; btn++) {
      buttons[btn].process(nowMs);
    }

    // look for triggers and reset to near the peak of the sine wave
    if (isTriggered && nowMs - lastTriggerMs > DEBOUNCE_DELAY) {
      // read the input on analog pin 0:
      int sensorValue = analogRead(trigPin);
     Serial.print("trig <-  ");
     Serial.println(sensorValue);
      // The analog reading goes from 0 - 1023
      if (sensorValue >= MIN_TRIG && lastRead <= MAX_TROUGH) {
        step = envStartStep;
        lastTriggerMs = nowMs;

        // toggle the LED ueach retrig
        digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
        on = !on;
       Serial.print("triggered; isSine: ");
       Serial.println(isSine);
      }
      lastRead = sensorValue;
    }

    int interpSteps = isTriggered ? defaultEnvSteps : defaultLFOSteps;
    if (useRatePin) {
      // set the LFO interpSteps based on the rate pot
      int rate = analogRead(ratePin);
      interpSteps = automap(rate, 1, 
        isTriggered ? maxEnvSteps : maxLfoSteps,
        isTriggered ? defaultEnvSteps : defaultLFOSteps);
//      Serial.print("Rate <- ");
//      Serial.print(rate);
//      Serial.print(": ");
//      Serial.println(interpSteps);
    }
    uint16_t i = 0;
    if (isLowRate) {
      interpSteps <<= 2;
    }
    if (isSine) {
      // Smooth it out with linear interpolation between samples
      const uint16_t nextStep = (step + 1) % 512;
      const uint16_t a = pgm_read_word(&DACLookup_FullSine_9Bit[step]);
      const uint16_t b = pgm_read_word(&DACLookup_FullSine_9Bit[nextStep]);
      uint16_t t;
      for (i = 0; i < interpSteps; i++) {
        t = lerp(a, b, i, interpSteps);
          Serial.println(t);
        dac.setVoltage(t, false);
      }
      step = nextStep;
    } else {
        // Adafruit triangle example
        // Run through the full 12-bit scale for a triangle wave
        if (!isTriggered && step < 4095) {
          for (i = 0; step < 4095 && i < interpSteps; i++, step++)
          {
            dac.setVoltage(step, false);
          }
        }
        if (step >= 4095) {
          for (; i < interpSteps && step > 0; i++, step--)
          {
            dac.setVoltage(step, false);
          }
        }
    }
}

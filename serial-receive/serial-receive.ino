/*********************************************************
This is a test of how I'll use SoftwareSerial to receive messages from 'touch-keyboard'
**********************************************************/

// Receiver Code
#include <SoftwareSerial.h>

#define LED LED_BUILTIN // shows if MIDI is being sent

SoftwareSerial link(10, 11); // Rx, Tx

const int nPads = 12;
bool touched[nPads];

void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  for (int i = 0; i < nPads; i++) {
    touched[i] = false;
  }

  Serial.begin(9600);
  link.begin(9600);
  
  while (!Serial) { // needed to keep leonardo/micro from starting too fast!
    delay(10);
  }
  
  Serial.println("Serial receive test"); 
}

unsigned int parseCharToHex(const char charX)
{
    if ('0' <= charX && charX <= '9') return charX - '0';
    if ('a' <= charX && charX <= 'f') return 10 + charX - 'a';
    if ('A' <= charX && charX <= 'F') return 10 + charX - 'A';
}

void loop() {
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
    } else {
      digitalWrite(LED, LOW);
    }
  }
}

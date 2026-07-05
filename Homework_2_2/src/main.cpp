#include <Arduino.h>

#define REL_PIN 10
#define REL_PIN_IN 11

#define RELAY_ON LOW
#define RELAY_OFF HIGH
#define BAUDRATE 115200

volatile bool relayChanged = false;
volatile unsigned long relayTime = 0;

unsigned long startTime;

int measurements = 0;
unsigned long sum = 0;

bool relayState = false;

void IRAM_ATTR relayInterrupt()
{
  relayTime = millis();
  relayChanged = true;
}


void setup() {
  Serial.begin(BAUDRATE);
  pinMode(REL_PIN, OUTPUT);
  pinMode (REL_PIN_IN, INPUT_PULLDOWN);
  digitalWrite(REL_PIN, RELAY_OFF);
  attachInterrupt(digitalPinToInterrupt(REL_PIN_IN), relayInterrupt, CHANGE);
    
  delay(2000);

  Serial.println("Start measurements");
}

void loop() {
  if (measurements >= 10)
  {
    Serial.println("----------------------");
    Serial.print("Average = ");
    Serial.print((float)sum / 10.0);
    Serial.println(" ms");

    sum = 0;
    measurements=0;
  }

  relayState = !relayState;
  startTime = millis();
  digitalWrite(REL_PIN, relayState==true?RELAY_ON: RELAY_OFF);
  relayChanged = false;
  while (!relayChanged)
  {
    //wait for interruption
  }

  unsigned long delta = relayTime - startTime;
  Serial.print("Measurement ");
  Serial.print(measurements + 1);
  Serial.print(": ");
  Serial.print(delta);
  Serial.println(" ms");

  sum += delta;
  measurements++;

  delay(1000);
}
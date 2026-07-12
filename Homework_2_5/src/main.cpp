#include <Arduino.h>

#define BAUDRATE 115200
#define RELAY_PIN 4
#define WORK_PIN 5
#define WAIT_PIN 6

volatile bool workState = false;

hw_timer_t *waitTimer = nullptr;

const unsigned long waitT = 3600;
const unsigned long workT = 900;

// const unsigned long waitT = 20; // VALUE FOR TEST
// const unsigned long workT = 5; // VALUE FOR TEST

unsigned long workedTime = 0;

void IRAM_ATTR onTimer()
{
  workedTime++;
  if (!workState && workedTime >= waitT)
  {
     Serial.printf("We are waiting. Time = %lu\n", workedTime);
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(WORK_PIN, HIGH);
    digitalWrite(WAIT_PIN, LOW);
    workState = true;
    workedTime = 0;
    
  }

  if (workState && workedTime >= workT)
  {
    Serial.printf("We are working. Time = %lu\n", workedTime);
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(WORK_PIN, LOW);
    digitalWrite(WAIT_PIN, HIGH);
    workState = false;
    workedTime = 0;
  }
}

void setup()
{
  Serial.begin(BAUDRATE);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(WORK_PIN, OUTPUT);
  pinMode(WAIT_PIN, OUTPUT);
  waitTimer = timerBegin(1000000);
  timerAttachInterrupt(waitTimer, &onTimer);
  timerAlarm(waitTimer, 1000000, true, 0);
  Serial.println("Start");
}

void loop()
{

}

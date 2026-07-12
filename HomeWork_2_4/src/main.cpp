// #include <Arduino.h>

// #define BTN_PIN 4
// #define DEBOUNCE_DELAY 50
// #define BAUDRATE 115200

// volatile uint32_t counter = 0;

// void IRAM_ATTR isrFunc(){
//   counter++;
//   Serial.printf("Count of clicks = %lu\n", counter);
// }

// void setup()
// {
//   Serial.begin(BAUDRATE);
//   pinMode(BTN_PIN, INPUT_PULLUP);
//   attachInterrupt(digitalPinToInterrupt(BTN_PIN), isrFunc, FALLING);
//   Serial.println("Start measuring");
// }

// void loop()
// {

// }

// // ------------------  2.4.2
// #include <Arduino.h>

// #define BTN_PIN 4
// #define DEBOUNCE_DELAY 50
// #define BAUDRATE 115200

// volatile uint32_t counter = 0;
// volatile bool isPressed = false;
// volatile uint32_t lastPressTime = 0;

// void IRAM_ATTR isrFunc()
// {
//   isPressed = true;
// }

// void setup()
// {
//   Serial.begin(BAUDRATE);
//   pinMode(BTN_PIN, INPUT_PULLUP);
//   attachInterrupt(digitalPinToInterrupt(BTN_PIN), isrFunc, FALLING);
//   Serial.println("Start measuring");
// }

// void loop()
// {
//   if (isPressed)
//   {
//     uint32_t now = millis();
//     if (now - lastPressTime >= 50)
//     {
//       lastPressTime = millis();
//       counter++;
//       Serial.printf("Count of clicks = %lu\n", counter);
//       isPressed = false;
//     }
//   }
// }

//------------------------- 2.4.3 ----------------
// #include <Arduino.h>

// #define BTN_PIN 4
// #define DEBOUNCE_DELAY 50
// #define BAUDRATE 115200

// volatile uint32_t counter = 0;
// volatile bool isPressed = false;
// volatile uint32_t lastPressTime = 0;

// void IRAM_ATTR isrFunc()
// {
//   isPressed = true;
// }

// void setup()
// {
//   Serial.begin(BAUDRATE);
//   pinMode(BTN_PIN, INPUT_PULLUP);
//   attachInterrupt(digitalPinToInterrupt(BTN_PIN), isrFunc, FALLING);
//   Serial.println("Start measuring");
// }

// void loop()
// {
//   if (isPressed)
//   {
//     if (digitalRead(BTN_PIN) == HIGH)
//     {
//       uint32_t now = millis();
//       if (now - lastPressTime >= 50)
//       {
//         lastPressTime = millis();
//         counter++;
//         Serial.printf("Count of clicks = %lu\n", counter);
//         isPressed = false;
//       }
//     }
//   }
// }

// ------------------------- 2.4.4 ----------------
#include <Arduino.h>

#define BTN_PIN 4
#define DEBOUNCE_DELAY 50
#define BAUDRATE 115200

volatile uint32_t counter = 0;
volatile bool isPressed = false;
unsigned long lastPressed = 0;
unsigned long pollingInterval = 10;
unsigned long lastDEBOUNCE_DELAY = 0;

void setup()
{
  Serial.begin(BAUDRATE);
  pinMode(BTN_PIN, INPUT_PULLUP);
  Serial.println("Start measuring");
}

void loop()
{
  unsigned long now = millis();
  if (now - lastPressed > pollingInterval)
  {
    lastPressed = now;
    unsigned long debounceNow = millis();
    if (debounceNow - lastDEBOUNCE_DELAY > DEBOUNCE_DELAY)
    {
      lastDEBOUNCE_DELAY = debounceNow;
      isPressed = digitalRead(BTN_PIN) == HIGH;
    }
  }

  if (isPressed)
  {
    if (digitalRead(BTN_PIN) == LOW)
    {
      counter++;
      isPressed = false;
      Serial.printf("Count of clicks = %lu\n", counter);
    }
  }
}

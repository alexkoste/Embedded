#include <Arduino.h>

#define LED_PIN1 4
#define LED_PIN2 5
#define LED_PIN3 6

volatile bool led_state1 = false;
volatile bool led_state2 = false;
volatile bool led_state3 = false;

volatile uint16_t last_change1 = 0;
volatile uint16_t last_change2 = 0;
volatile uint16_t last_change3 = 0;

void setup()
{
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(LED_PIN3, OUTPUT);
}

void loop()
{
  if (  millis() - last_change1 >= 200)
  {
    last_change1 = millis();
    led_state1 = !led_state1;
    digitalWrite(LED_PIN1, led_state1);
  }

  if (  millis() - last_change2 >= 500)
  {
    last_change2 = millis();
    led_state2 = !led_state2;
    digitalWrite(LED_PIN2, led_state2);
  }

  if (  millis() - last_change3 >= 1000)
  {
    last_change3 = millis();
    led_state3 = !led_state3;
    digitalWrite(LED_PIN3, led_state3);
  }
}

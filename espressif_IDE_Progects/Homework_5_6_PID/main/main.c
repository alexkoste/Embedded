#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

// Піни підключення до L293D
#define MOTOR_PWM_GPIO GPIO_NUM_18 // Підключено до EN1 (ШІМ швидкість)
#define MOTOR_IN1_GPIO GPIO_NUM_8  // Підключено до IN1 (Напрямок 1)
#define MOTOR_IN2_GPIO GPIO_NUM_9  // Підключено до IN2 (Напрямок 2)

// Піни енкодера та кнопок
#define ENCODER_S1_GPIO GPIO_NUM_4 // Пін S1 (CLK)
#define ENCODER_S2_GPIO GPIO_NUM_5 // Пін S2 (DT)
#define ENCODER_KEY_GPIO                                                       \
	GPIO_NUM_6					 // Кнопка KEY (зупинка + 90° за годинниковою)
#define STOP_BTN_GPIO GPIO_NUM_7 // Кнопка екстреного стопу

/* =========================================================================
 * НАЛАШТУВАННЯ ПІД-КОЕФІЦІЄНТІВ (PID Coefficients)
 * ========================================================================= */
#define PID_KP 3.5f
#define PID_KI 0.02f
#define PID_KD 0.3f

// Константи системи
#define TICKS_PER_90_DEG                                                       \
	130 // Імпульсів на 90 градусів (підлаштуйте під свій редуктор)
#define MAX_PWM_DUTY 1023 // Максимальне значення ШІМ (10-біт)
#define MIN_PWM_DUTY 200  // Мертва зона двигуна

// Змінні стану
static volatile int32_t encoder_count =
	0; // Реальне положення (якщо є датчик на моторі)
static volatile int32_t setpoint = 0; // Цільове положення
static uint8_t last_s1_state = 1;

// Переривання для зчитування ручки енкодера (вільне обертання)
static void IRAM_ATTR encoder_gpio_isr_handler(void *arg) {
	int s1 = gpio_get_level(ENCODER_S1_GPIO);
	int s2 = gpio_get_level(ENCODER_S2_GPIO);

	if (s1 != last_s1_state) {
		if (s2 != s1) {
			setpoint += 5; // Обертання в один бік
		} else {
			setpoint -= 5; // Обертання в інший бік
		}
	}
	last_s1_state = s1;
}

// Переривання для кнопки KEY: зупиняє двигун і додає +90° за годинниковою
// стрілкою
static void IRAM_ATTR button_key_isr_handler(void *arg) {
	static int64_t last_click_time = 0;
	int64_t now = esp_timer_get_time() / 1000;

	if (now - last_click_time > 400) {
		// Додаємо 90 градусів саме до поточної цілі (setpoint),
		// або до encoder_count, якщо потрібно гарантовано зрушити з місця:
		setpoint = setpoint + TICKS_PER_90_DEG;

		last_click_time = now;
	}
}

// Переривання для кнопки екстреного стопу
static void IRAM_ATTR stop_button_isr_handler(void *arg) {
	static int64_t last_click_time = 0;
	int64_t now = esp_timer_get_time() / 1000;
	if (now - last_click_time > 200) {
		setpoint = encoder_count; // Миттєва зупинка
		last_click_time = now;
	}
}

// Ініціалізація пінів та ШІМ
static void motor_init(void) {
	gpio_config_t io_conf = {.pin_bit_mask = (1ULL << MOTOR_IN1_GPIO) |
											 (1ULL << MOTOR_IN2_GPIO),
							 .mode = GPIO_MODE_OUTPUT,
							 .pull_up_en = GPIO_PULLUP_DISABLE,
							 .pull_down_en = GPIO_PULLDOWN_DISABLE,
							 .intr_type = GPIO_INTR_DISABLE};
	gpio_config(&io_conf);

	ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
									  .timer_num = LEDC_TIMER_0,
									  .duty_resolution = LEDC_TIMER_10_BIT,
									  .freq_hz = 20000,
									  .clk_cfg = LEDC_AUTO_CLK};
	ledc_timer_config(&ledc_timer);

	ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
										  .channel = LEDC_CHANNEL_0,
										  .timer_sel = LEDC_TIMER_0,
										  .intr_type = LEDC_INTR_DISABLE,
										  .gpio_num = MOTOR_PWM_GPIO,
										  .duty = 0,
										  .hpoint = 0};
	ledc_channel_config(&ledc_channel);
}

// Керування двигуном через L293D (з урахуванням реверса)
static void set_motor_control(float control_signal) {
	int duty = (int)(control_signal);

	if (duty > 0) {
		gpio_set_level(MOTOR_IN1_GPIO, 1);
		gpio_set_level(MOTOR_IN2_GPIO, 0);
	} else if (duty < 0) {
		gpio_set_level(MOTOR_IN1_GPIO, 0);
		gpio_set_level(MOTOR_IN2_GPIO, 1);
		duty = -duty;
	} else {
		gpio_set_level(MOTOR_IN1_GPIO, 0);
		gpio_set_level(MOTOR_IN2_GPIO, 0);
		ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
		return;
	}

	if (duty > MAX_PWM_DUTY)
		duty = MAX_PWM_DUTY;
	if (duty < MIN_PWM_DUTY)
		duty = MIN_PWM_DUTY;

	ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// ПІД-задача FreeRTOS (виконується кожні 10 мс)
void pid_task(void *pvParameters) {
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100 Гц
	xLastWakeTime = xTaskGetTickCount();

	float integral = 0;
	float previous_error = 0;

	while (1) {
		int32_t current_position = encoder_count;
		float error = (float)setpoint - (float)current_position;

		integral += error;
		if (integral > 1500)
			integral = 1500;
		if (integral < -1500)
			integral = -1500;

		float derivative = error - previous_error;
		float output =
			(PID_KP * error) + (PID_KI * integral) + (PID_KD * derivative);

		previous_error = error;

		set_motor_control(output);

		vTaskDelayUntil(&xLastWakeTime, xFrequency);
	}
}

void app_main(void) {
	motor_init();

	// Налаштування пінів енкодера (S1, S2)
	gpio_config_t enc_conf = {.pin_bit_mask = (1ULL << ENCODER_S1_GPIO) |
											  (1ULL << ENCODER_S2_GPIO),
							  .mode = GPIO_MODE_INPUT,
							  .pull_up_en = GPIO_PULLUP_ENABLE,
							  .pull_down_en = GPIO_PULLDOWN_DISABLE,
							  .intr_type = GPIO_INTR_ANYEDGE};
	gpio_config(&enc_conf);

	// Налаштування пінів кнопок (KEY та STOP) з підтяжкою до живлення
	gpio_config_t btn_conf = {.pin_bit_mask = (1ULL << ENCODER_KEY_GPIO) |
											  (1ULL << STOP_BTN_GPIO),
							  .mode = GPIO_MODE_INPUT,
							  .pull_up_en = GPIO_PULLUP_ENABLE,
							  .pull_down_en = GPIO_PULLDOWN_DISABLE,
							  .intr_type = GPIO_INTR_NEGEDGE};
	gpio_config(&btn_conf);

	// Реєстрація обробників переривань
	gpio_install_isr_service(0);
	gpio_isr_handler_add(ENCODER_S1_GPIO, encoder_gpio_isr_handler, NULL);
	gpio_isr_handler_add(ENCODER_KEY_GPIO, button_key_isr_handler, NULL);
	gpio_isr_handler_add(STOP_BTN_GPIO, stop_button_isr_handler, NULL);

	// Запуск ПІД-регулятора
	xTaskCreate(pid_task, "pid_task", 2048, NULL, 5, NULL);

	printf("Servo System Initialized: Encoder + KEY (+90 deg).\n");
}
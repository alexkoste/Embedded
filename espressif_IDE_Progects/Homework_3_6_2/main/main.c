#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/gpio_num.h"

static const char *TAG = "SAFE_LOCK";


#define GPIO_ENCODER_CLK   GPIO_NUM_4
#define GPIO_ENCODER_DT    GPIO_NUM_6
#define GPIO_ENCODER_SW    GPIO_NUM_5
#define GPIO_RELAY         GPIO_NUM_7
#define GPIO_BUZZER        GPIO_NUM_10


#define PIN_LENGTH         4
static const int SECRET_PIN[PIN_LENGTH] = {1, 3, 3, 7};
#define MAX_ATTEMPTS       3

typedef enum {
    DIR_NONE = 0,
    DIR_CW   = 1,
    DIR_CCW  = -1
} dir_t;


static int attempts_left = MAX_ATTEMPTS;
static int entered_code[PIN_LENGTH];
static int current_digit_idx = 0;
static int current_digit_val = 0;
static dir_t last_dir = DIR_NONE;
static bool system_locked = false;


static void reset_attempt(bool count_as_attempt);
static void check_code(void);
static void trigger_alarm(void);
static void trigger_success(void);

// Initialize GPIO Peripherals
static void init_hardware(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_ENCODER_SW) | (1ULL << GPIO_ENCODER_DT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Encoder Push Button with Pull-Up
    io_conf.pin_bit_mask = (1ULL << GPIO_ENCODER_CLK);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Relay and Buzzer Outputs
    io_conf.pin_bit_mask = (1ULL << GPIO_RELAY) | (1ULL << GPIO_BUZZER);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(GPIO_RELAY, 0);
    gpio_set_level(GPIO_BUZZER, 0);
}


static void reset_attempt(bool count_as_attempt) {
    if (count_as_attempt) {
        attempts_left--;
        ESP_LOGW(TAG, "\n[RESET] Attempt reset by button! Attempts left: %d", attempts_left);
    }

    if (attempts_left <= 0) {
        trigger_alarm();
        return;
    }

    current_digit_idx = 0;
    current_digit_val = 0;
    last_dir = DIR_NONE;

    printf("\nEnter PIN [%d/%d]: ", MAX_ATTEMPTS - attempts_left + 1, MAX_ATTEMPTS);
    printf("%d", current_digit_val);
    fflush(stdout);
}


static void check_code(void) {
    printf("\n");
    bool correct = true;
    for (int i = 0; i < PIN_LENGTH; i++) {
        if (entered_code[i] != SECRET_PIN[i]) {
            correct = false;
            break;
        }
    }

    if (correct) {
        trigger_success();
    } else {
        attempts_left--;
        ESP_LOGE(TAG, "Incorrect PIN! Attempts left: %d", attempts_left);
        if (attempts_left <= 0) {
            trigger_alarm();
        } else {
            reset_attempt(false);
        }
    }
}


static void trigger_alarm(void) {
    system_locked = true;
    ESP_LOGE(TAG, "!!! SAFE LOCKED OUT. NO ATTEMPTS REMAINING !!!");
    
  
    while (1) {
        gpio_set_level(GPIO_BUZZER, 1);
        vTaskDelay(pdMS_TO_TICKS(150));
        gpio_set_level(GPIO_BUZZER, 0);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}


static void trigger_success(void) {
    ESP_LOGI(TAG, ">>> ACCESS GRANTED! SAFE UNLOCKED <<<");
    
    // Activate relay (unlock mechanism)
    gpio_set_level(GPIO_RELAY, 1);

    // Success chime sequence
    int tones[] = {100, 100, 250};
    for (int i = 0; i < 3; i++) {
        gpio_set_level(GPIO_BUZZER, 1);
        vTaskDelay(pdMS_TO_TICKS(tones[i]));
        gpio_set_level(GPIO_BUZZER, 0);
        vTaskDelay(pdMS_TO_TICKS(80));
    }

    vTaskDelay(pdMS_TO_TICKS(5000)); // Keep relay active for 5 seconds
    gpio_set_level(GPIO_RELAY, 0);
    
    attempts_left = MAX_ATTEMPTS;
    reset_attempt(false);
}

void app_main(void) {
    init_hardware();

    ESP_LOGI(TAG, "Safe Lock System started.");
    reset_attempt(false);

    int last_clk_state = gpio_get_level(GPIO_ENCODER_CLK);
    bool sw_last_state = true;

    while (!system_locked) {
        // 1. Read encoder push button (SW)
        bool sw_state = gpio_get_level(GPIO_ENCODER_CLK);
        if (sw_last_state && !sw_state) { 
            vTaskDelay(pdMS_TO_TICKS(50)); 
            if (!gpio_get_level(GPIO_ENCODER_CLK)) {
                reset_attempt(true);
            }
        }
        sw_last_state = sw_state;

        // 2. Read encoder shaft rotation
        int clk_state = gpio_get_level(GPIO_ENCODER_SW);
        if (clk_state != last_clk_state && clk_state == 0) { 
            int dt_state = gpio_get_level(GPIO_ENCODER_DT);
            dir_t new_dir = (dt_state != clk_state) ? DIR_CW : DIR_CCW;

            if (last_dir == DIR_NONE) {
                // First rotation detected
                last_dir = new_dir;
                current_digit_val = (current_digit_val + 1) % 10;
                printf("\b%d", current_digit_val);
                fflush(stdout);
            } else if (new_dir == last_dir) {
                // Rotation in same direction -> Increment current digit
                current_digit_val = (current_digit_val + 1) % 10;
                printf("\b%d", current_digit_val);
                fflush(stdout);
            } else {
                // Direction changed -> Confirm previous digit and advance
                entered_code[current_digit_idx] = current_digit_val;
                current_digit_idx++;

                if (current_digit_idx >= PIN_LENGTH) {
                    check_code();
                } else {
                    last_dir = new_dir;
                    current_digit_val = 0; 
                    printf(" %d", current_digit_val);
                    fflush(stdout);
                }
            }
        }
        last_clk_state = clk_state;
        vTaskDelay(pdMS_TO_TICKS(2)); 
    }
}
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SERVO_ENCODER";


#define SERVO_GPIO       18
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define ENCODER_CLK_GPIO 5
#define ENCODER_DT_GPIO  6
#define BUTTON_GPIO      4
#define BUZZER_GPIO      10


#define SERVO_MIN_PULSEWIDTH_US 500   
#define SERVO_MAX_PULSEWIDTH_US 2500  
#define SERVO_MAX_ANGLE         180.0f


#define STEP_COARSE             10.0f
#define STEP_FINE               5.0f
#define LONG_PRESS_TIME_US      800000 


static float current_angle =  0.0f;
static bool is_fine_mode = false;
static mcpwm_cmpr_handle_t comparator = NULL;


static void play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    uint32_t period_us = 1000000 / freq_hz;
    uint32_t half_period_us = period_us / 2;
    uint32_t cycles = (freq_hz * duration_ms) / 1000;

    for (uint32_t i = 0; i < cycles; i++) {
        gpio_set_level(BUZZER_GPIO, 1);
        esp_rom_delay_us(half_period_us);
        gpio_set_level(BUZZER_GPIO, 0);
        esp_rom_delay_us(half_period_us);
    }
}


static uint32_t angle_to_compare(float angle) {
    return (uint32_t)(SERVO_MIN_PULSEWIDTH_US + 
           ((angle / SERVO_MAX_ANGLE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US)));
}


static void set_servo_angle(float angle) {
    current_angle = angle;
    uint32_t pulse_width = angle_to_compare(current_angle);
    mcpwm_comparator_set_compare_value(comparator, pulse_width);
	
}


static void init_servo(void) {
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, 
        .period_ticks = 20000,    
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t oper_config = { .group_id = 0 };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    mcpwm_comparator_config_t cmpr_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &cmpr_config, &comparator));

    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t gen_config = { .gen_gpio_num = SERVO_GPIO };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_config, &generator));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    set_servo_angle(current_angle);
}


static pcnt_unit_handle_t init_encoder(void) {
    pcnt_unit_handle_t pcnt_unit = NULL;
    pcnt_unit_config_t unit_config = {
        .low_limit = -100,
        .high_limit = 100,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_channel_handle_t chan_a = NULL;
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = ENCODER_CLK_GPIO,
        .level_gpio_num = ENCODER_DT_GPIO,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &chan_a));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    return pcnt_unit;
}

void app_main(void) {
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << BUZZER_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    init_servo();
    pcnt_unit_handle_t pcnt_unit = init_encoder();

    int last_pulse_count = 0;
    bool last_btn_state = true;
    int64_t btn_press_time = 0;

    ESP_LOGI(TAG, "System launched. IInitial angle: %.1f", current_angle);

    while (1) {
        
        int pulse_count = 0;
        pcnt_unit_get_count(pcnt_unit, &pulse_count);
        int delta = pulse_count - last_pulse_count;

        if (delta != 0) {
            last_pulse_count = pulse_count;
            float step = is_fine_mode ? STEP_FINE : STEP_COARSE;
            float target_angle = current_angle + (delta * step);

            if (target_angle > SERVO_MAX_ANGLE) {
                if (current_angle < SERVO_MAX_ANGLE) {
                    set_servo_angle(SERVO_MAX_ANGLE);
                }
                play_tone(1200, 80); 
            } else if (target_angle < 0.0f) {
                if (current_angle > 0.0f) {
                    set_servo_angle(0.0f);
                }
                play_tone(1200, 80); 
            } else {
                set_servo_angle(target_angle);
            }
            ESP_LOGI(TAG, "Angle: %.1f | Mode: %s", current_angle, is_fine_mode ? "AIM" : "Original");
        }

   
        bool btn_state = gpio_get_level(BUTTON_GPIO);

        if (last_btn_state && !btn_state) { 
    
            btn_press_time = esp_timer_get_time();
        } else if (!last_btn_state && btn_state) { 
           
            int64_t press_duration = esp_timer_get_time() - btn_press_time;

            if (press_duration >= LONG_PRESS_TIME_US) {
                
                set_servo_angle(90.0f);
                play_tone(2000, 150);
                ESP_LOGI(TAG, "Move to the center (90 deg)");
            } else if (press_duration > 50000) { 
                
                is_fine_mode = !is_fine_mode;
                play_tone(is_fine_mode ? 1500 : 800, 50);
                ESP_LOGI(TAG, "Change: %s", is_fine_mode ? "STEP_FINE (5deg)" : "STEP_COARSE (10deg)");
            }
        }
        last_btn_state = btn_state;

        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}
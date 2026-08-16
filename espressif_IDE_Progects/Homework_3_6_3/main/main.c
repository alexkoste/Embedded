#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"


#define ENCODER_GPIO_A     5
#define ENCODER_GPIO_B     6
#define ENCODER_GPIO_BTN   4

#define PCNT_HIGH_LIMIT    10000
#define PCNT_LOW_LIMIT    -10000

typedef enum {
    STATE_OPERAND1,
    STATE_OPERATION,
    STATE_OPERAND2,
    STATE_RESULT
} calc_state_t;

static const char OPERATIONS[] = {'+', '-', '*', '/'};


void erase_chars(int count) {
    for (int i = 0; i < count; i++) {
        printf("\b \b");
    }
    fflush(stdout);
}

void app_main(void)
{
   
    gpio_config_t btn_config = {
        .pin_bit_mask = (1ULL << ENCODER_GPIO_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_config);

    
    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit = PCNT_LOW_LIMIT,
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = { .edge_gpio_num = ENCODER_GPIO_A, .level_gpio_num = ENCODER_GPIO_B };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    pcnt_chan_config_t chan_b_config = { .edge_gpio_num = ENCODER_GPIO_B, .level_gpio_num = ENCODER_GPIO_A };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));


    calc_state_t state = STATE_OPERAND1;
    int op1 = 0, op2 = 0;
    int op_idx = 0; 
    
    int last_pcnt_val = 0;
    int last_btn_state = 1;
    int print_len = 0; 

    printf("\n--- Calculator Started ---\n> ");
    print_len = printf("%d", op1);
    fflush(stdout);

    while (1) {

        int current_pcnt_val = 0;
        pcnt_unit_get_count(pcnt_unit, &current_pcnt_val);
        
        
        int delta = (current_pcnt_val - last_pcnt_val) / 4;

        if (delta != 0) {
            last_pcnt_val += delta * 4;

            switch (state) {
                case STATE_OPERAND1:
                    op1 += delta;
                    erase_chars(print_len);
                    print_len = printf("%d", op1);
                    break;

                case STATE_OPERATION:
                    op_idx = (op_idx + delta) % 4;
                    if (op_idx < 0) op_idx += 4;
                    erase_chars(print_len);
                    print_len = printf("%c ", OPERATIONS[op_idx]);
                    break;

                case STATE_OPERAND2:
                    op2 += delta;
                    erase_chars(print_len);
                    print_len = printf("%d", op2);
                    break;

                default:
                    break;
            }
            fflush(stdout);
        }

        
        int btn_state = gpio_get_level(ENCODER_GPIO_BTN);
        if (last_btn_state == 1 && btn_state == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); 

            switch (state) {
                case STATE_OPERAND1:
                    state = STATE_OPERATION;
                    print_len = printf(" %c ", OPERATIONS[op_idx]);
                    print_len = 2; 
                    break;

                case STATE_OPERATION:
                    state = STATE_OPERAND2;
                    print_len = printf("%d", op2);
                    break;

                case STATE_OPERAND2:
                    state = STATE_RESULT;
                    printf(" = ");
                    

                    switch (OPERATIONS[op_idx]) {
                        case '+': printf("%d\n", op1 + op2); break;
                        case '-': printf("%d\n", op1 - op2); break;
                        case '*': printf("%d\n", op1 * op2); break;
                        case '/': 
                            if (op2 != 0) printf("%.2f\n", (float)op1 / op2);
                            else printf("Error (Division by zero)\n");
                            break;
                    }


                    vTaskDelay(pdMS_TO_TICKS(1500));
                    op1 = 0; op2 = 0; op_idx = 0;
                    state = STATE_OPERAND1;
                    printf("\n> ");
                    print_len = printf("%d", op1);
                    break;

                default:
                    break;
            }
            fflush(stdout);
        }
        last_btn_state = btn_state;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
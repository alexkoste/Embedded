#include "include/display_manager.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define TAG "DISPLAY_MGR"
#define SSD1306_I2C_ADDR 0x3C
#define BTN_NEXT_PIN GPIO_NUM_5

extern int get_total_devices(void);
extern void get_device_info(int index, char *name, float *temp, float *pres,
							float *hum, bool *active);
static int current_view = 0;

static const uint8_t font5x7[][5] = {['0'] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
									 ['1'] = {0x00, 0x42, 0x7F, 0x40, 0x00},
									 ['2'] = {0x42, 0x61, 0x51, 0x49, 0x46},
									 ['3'] = {0x21, 0x41, 0x45, 0x4B, 0x31},
									 ['4'] = {0x18, 0x14, 0x12, 0x7F, 0x10},
									 ['5'] = {0x27, 0x45, 0x45, 0x45, 0x39},
									 ['6'] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
									 ['7'] = {0x01, 0x71, 0x09, 0x05, 0x03},
									 ['8'] = {0x36, 0x49, 0x49, 0x49, 0x36},
									 ['9'] = {0x06, 0x49, 0x49, 0x29, 0x1E},
									 ['.'] = {0x00, 0x60, 0x60, 0x00, 0x00},
									 ['='] = {0x14, 0x14, 0x14, 0x14, 0x14},
									 ['%'] = {0x23, 0x13, 0x08, 0x64, 0x62},
									 ['T'] = {0x01, 0x01, 0x7F, 0x01, 0x01},
									 ['H'] = {0x7F, 0x08, 0x08, 0x08, 0x7F},
									 ['P'] = {0x7F, 0x09, 0x09, 0x09, 0x06},
									 ['C'] = {0x3E, 0x41, 0x41, 0x41, 0x22},
									 ['h'] = {0x7F, 0x08, 0x04, 0x04, 0x78},
									 ['p'] = {0xFC, 0x24, 0x24, 0x24, 0x18},
									 ['a'] = {0x20, 0x54, 0x54, 0x54, 0x78},
									 [' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
									 ['d'] = {0x38, 0x44, 0x44, 0x48, 0x7F},
									 ['N'] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
									 // Латиниця для назв станцій
									 ['A'] = {0x7E, 0x09, 0x09, 0x09, 0x7E},
									 ['B'] = {0x7F, 0x49, 0x49, 0x49, 0x36},
									 ['b'] = {0x7F, 0x48, 0x44, 0x44, 0x38},
									 ['D'] = {0x7F, 0x41, 0x41, 0x22, 0x1C},
									 ['e'] = {0x38, 0x54, 0x54, 0x54, 0x18},
									 ['G'] = {0x3E, 0x41, 0x49, 0x49, 0x7A},
									 ['i'] = {0x00, 0x44, 0x7D, 0x40, 0x00},
									 ['M'] = {0x7F, 0x02, 0x0C, 0x02, 0x7F},
									 ['n'] = {0x7C, 0x08, 0x04, 0x04, 0x78},
									 ['o'] = {0x38, 0x44, 0x44, 0x44, 0x38},
									 ['r'] = {0x7C, 0x08, 0x04, 0x04, 0x08},
									 ['s'] = {0x48, 0x54, 0x54, 0x54, 0x20},
									 ['t'] = {0x04, 0x3F, 0x44, 0x40, 0x20},
									 ['v'] = {0x1C, 0x20, 0x40, 0x20, 0x1C},
									 ['y'] = {0x4C, 0x90, 0x90, 0x90, 0x7C}};

static uint8_t ssd1306_buffer[128 * 8];

static void ssd1306_send_cmd(uint8_t cmd) {
	uint8_t buf[2] = {0x00, cmd};
	i2c_master_write_to_device(I2C_NUM_0, SSD1306_I2C_ADDR, buf, 2,
							   pdMS_TO_TICKS(1000));
}

static void ssd1306_flush(void) {
	ssd1306_send_cmd(0x21);
	ssd1306_send_cmd(0);
	ssd1306_send_cmd(127);
	ssd1306_send_cmd(0x22);
	ssd1306_send_cmd(0);
	ssd1306_send_cmd(7);
	uint8_t *packet = malloc(128 * 8 + 1);
	packet[0] = 0x40;
	memcpy(packet + 1, ssd1306_buffer, 128 * 8);
	i2c_master_write_to_device(I2C_NUM_0, SSD1306_I2C_ADDR, packet, 128 * 8 + 1,
							   pdMS_TO_TICKS(1000));
	free(packet);
}

void display_init(void) {
	gpio_config_t io_conf = {
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pin_bit_mask = (1ULL << BTN_NEXT_PIN),
	};
	gpio_config(&io_conf);

	uint8_t init_cmds[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D,
						   0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF,
						   0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
	for (int i = 0; i < sizeof(init_cmds); i++)
		ssd1306_send_cmd(init_cmds[i]);
	memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
	ssd1306_flush();
}

static void draw_char(int x, int y, char c) {
	if (c < 32 || c > 126)
		c = '?';
	const uint8_t *glyph = font5x7[(int)c];
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 7; j++) {
			if (glyph[i] & (1 << j)) {
				int px = x + i, py = y + j;
				if (px >= 0 && px < 128 && py >= 0 && py < 64) {
					ssd1306_buffer[px + (py / 8) * 128] |= (1 << (py % 8));
				}
			}
		}
	}
}

static void draw_string(int x, int y, const char *str) {
	while (*str) {
		draw_char(x, y, *str);
		x += 6;
		str++;
	}
}

void display_refresh(void) {
	if (gpio_get_level(BTN_NEXT_PIN) == 0) {
		int total = get_total_devices();
		if (total > 0)
			current_view = (current_view + 1) % total;
		vTaskDelay(pdMS_TO_TICKS(250));
	}

	char name[32];
	float temp, pres, hum;
	bool active;
	get_device_info(current_view, name, &temp, &pres, &hum, &active);

	memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));

	char line_station[48], line_t[48], line_h[48], line_p[48];

	snprintf(line_station, sizeof(line_station), "%s", name);
	snprintf(line_t, sizeof(line_t), "T = %.1f C", temp);
	snprintf(line_h, sizeof(line_h), "H = %.1f %%", hum);
	snprintf(line_p, sizeof(line_p), "P = %.1f hPa", pres);

	// Рядок 0: Назва станції
	draw_string(0, 0, line_station);
	// Рядок 1: Температура
	draw_string(0, 13, line_t);
	// Рядок 2: Вологість
	draw_string(0, 26, line_h);
	// Рядок 3: Тиск
	draw_string(0, 39, line_p);

	// Індикатор сторінок у правому нижньому кутку
	char page_str[48];
	snprintf(page_str, sizeof(page_str), "%s/%d", name, get_total_devices());

	// Зміщуємо трохи лівіше (наприклад, на x = 60), щоб назва станції повністю
	// помістилася внизу
	draw_string(60, 54, page_str);

	ssd1306_flush();
}
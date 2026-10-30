#include "include/sensor_manager.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "SENSOR_MGR"

#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define BME280_SENSOR_ADDR 0x76

typedef struct {
	uint16_t dig_T1;
	int16_t dig_T2;
	int16_t dig_T3;
	uint16_t dig_P1;
	int16_t dig_P2;
	int16_t dig_P3;
	int16_t dig_P4;
	int16_t dig_P5;
	int16_t dig_P6;
	int16_t dig_P7;
	int16_t dig_P8;
	int16_t dig_P9;
	uint8_t dig_H1;
	int16_t dig_H2;
	uint8_t dig_H3;
	int16_t dig_H4;
	int16_t dig_H5;
	int8_t dig_H6;
} bme280_calib_data_t;

static bme280_calib_data_t calib;
static int32_t t_fine;

static esp_err_t bme280_register_read(uint8_t r_addr, uint8_t *data,
									  size_t len) {
	return i2c_master_write_read_device(I2C_MASTER_NUM, BME280_SENSOR_ADDR,
										&r_addr, 1, data, len,
										pdMS_TO_TICKS(1000));
}

static esp_err_t bme280_register_write_byte(uint8_t r_addr, uint8_t val) {
	uint8_t write_buf[2] = {r_addr, val};
	return i2c_master_write_to_device(I2C_MASTER_NUM, BME280_SENSOR_ADDR,
									  write_buf, sizeof(write_buf),
									  pdMS_TO_TICKS(1000));
}

static void read_calibration_data(void) {
	uint8_t buff[24];
	bme280_register_read(0x88, buff, 24);
	calib.dig_T1 = (buff[1] << 8) | buff[0];
	calib.dig_T2 = (buff[3] << 8) | buff[2];
	calib.dig_T3 = (buff[5] << 8) | buff[4];
	calib.dig_P1 = (buff[7] << 8) | buff[6];
	calib.dig_P2 = (buff[9] << 8) | buff[8];
	calib.dig_P3 = (buff[11] << 8) | buff[10];
	calib.dig_P4 = (buff[13] << 8) | buff[12];
	calib.dig_P5 = (buff[15] << 8) | buff[14];
	calib.dig_P6 = (buff[17] << 8) | buff[16];
	calib.dig_P7 = (buff[19] << 8) | buff[18];
	calib.dig_P8 = (buff[21] << 8) | buff[20];
	calib.dig_P9 = (buff[23] << 8) | buff[22];

	bme280_register_read(0xA1, &calib.dig_H1, 1);

	uint8_t h_buff[7];
	bme280_register_read(0xE1, h_buff, 7);
	calib.dig_H2 = (h_buff[1] << 8) | h_buff[0];
	calib.dig_H3 = h_buff[2];
	calib.dig_H4 = (h_buff[3] << 4) | (h_buff[4] & 0x0F);
	calib.dig_H5 = (h_buff[5] << 4) | (h_buff[4] >> 4);
	calib.dig_H6 = h_buff[6];
}

void sensor_init(void) {
	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_MASTER_SDA_IO,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_io_num = I2C_MASTER_SCL_IO,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_MASTER_FREQ_HZ,
	};
	i2c_param_config(I2C_MASTER_NUM, &conf);
	i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

	uint8_t chip_id = 0;
	bme280_register_read(0xD0, &chip_id, 1);
	if (chip_id != 0x60) {
		ESP_LOGE(TAG, "BME280 not found! Chip ID: 0x%X", chip_id);
		return;
	}

	read_calibration_data();

	// 1. Спочатку переводимо датчик у SLEEP mode (запис 0x00 в 0xF4)
	bme280_register_write_byte(0xF4, 0x00);

	// 2. Налаштовуємо вологість (oversampling x1) у регістрі 0xF2
	bme280_register_write_byte(0xF2, 0x01);

	// 3. Налаштовуємо температуру (x1), тиск (x1) та переводимо в NORMAL mode
	// (0x27)
	bme280_register_write_byte(0xF4, 0x27);

	// 4. Додаткові параметри фільтрації (config)
	bme280_register_write_byte(0xF5, 0x00);

	ESP_LOGI(TAG,
			 "BME280 initialized successfully with correct sleep sequence");
}

sensor_data_t sensor_read(void) {
	sensor_data_t data = {23.0f, 1013.25f, 50.0f};
	uint8_t rx_data[8];

	if (bme280_register_read(0xF7, rx_data, 8) != ESP_OK)
		return data;

	int32_t adc_P = ((uint32_t)rx_data[0] << 12) | ((uint32_t)rx_data[1] << 4) |
					(rx_data[2] >> 4);
	int32_t adc_T = ((uint32_t)rx_data[3] << 12) | ((uint32_t)rx_data[4] << 4) |
					(rx_data[5] >> 4);
	int32_t adc_H = ((uint32_t)rx_data[6] << 8) | rx_data[7];

	int32_t var1, var2;
	var1 = (((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) *
			   ((int32_t)calib.dig_T2) >>
		   11;
	var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) *
			  ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >>
			 12) *
			((int32_t)calib.dig_T3)) >>
		   14;
	t_fine = var1 + var2;
	data.temperature = (float)((t_fine * 5 + 128) >> 8) / 100.0f;

	int64_t p_var1, p_var2, p;
	p_var1 = ((int64_t)t_fine) - 128000;
	p_var2 = p_var1 * p_var1 * (int64_t)calib.dig_P6;
	p_var2 = p_var2 + ((p_var1 * (int64_t)calib.dig_P5) << 17);
	p_var2 = p_var2 + (((int64_t)calib.dig_P4) << 35);
	p_var1 = ((p_var1 * p_var1 * (int64_t)calib.dig_P3) >> 8) +
			 ((p_var1 * (int64_t)calib.dig_P2) << 12);
	p_var1 = (((((int64_t)1) << 47) + p_var1)) * ((int64_t)calib.dig_P1) >> 33;
	if (p_var1 == 0) {
		data.pressure = 0.0f;
	} else {
		p = 1048576 - adc_P;
		p = (((p << 31) - p_var2) * 3125) / p_var1;
		p_var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
		p_var2 = (((int64_t)calib.dig_P8) * p) >> 19;
		p = ((p + p_var1 + p_var2) >> 8) + (((int64_t)calib.dig_P7) << 4);
		data.pressure = (float)p / 256.00f / 100.0f;
	}

	int32_t v_x1_u32r;
	v_x1_u32r = (t_fine - ((int32_t)76800));
	int32_t adc_H_s32 = (int32_t)adc_H;

	v_x1_u32r = (((((adc_H_s32 << 14) - (((int32_t)calib.dig_H4) << 20) -
					(((int32_t)calib.dig_H5) * v_x1_u32r)) +
				   (int32_t)16384) >>
				  15) *
				 (((((((v_x1_u32r * ((int32_t)calib.dig_H6)) >> 10) *
					  (((v_x1_u32r * ((int32_t)calib.dig_H3)) >> 11) +
					   (int32_t)32768)) >>
					 10) +
					(int32_t)2097152) *
					   ((int32_t)calib.dig_H2) +
				   (int32_t)8192) >>
				  14));

	v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
							   ((int32_t)calib.dig_H1)) >>
							  4));
	v_x1_u32r = v_x1_u32r < 0 ? 0 : v_x1_u32r;
	v_x1_u32r = v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r;
	data.humidity = (float)(v_x1_u32r >> 12) / 1024.0f;

	return data;
}

uint32_t bme280_compensate_humidity(int32_t adc_H, uint8_t dig_H1,
									int16_t dig_H2, uint8_t dig_H3,
									int16_t dig_H4, int16_t dig_H5,
									int8_t dig_H6) {
	int32_t v_x1_u32r;

	v_x1_u32r = (t_fine - (int32_t)76800);

	v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) -
					(((int32_t)dig_H5) * v_x1_u32r)) +
				   (int32_t)16384) >>
				  15) *
				 (((((((v_x1_u32r * (int32_t)dig_H6) >> 10) *
					  (((v_x1_u32r * (int32_t)3978) >> 14) + (int32_t)16384)) >>
					 10) +
					(int32_t)2097152) *
					   (int32_t)dig_H2 +
				   (int32_t)8192) >>
				  14));

	v_x1_u32r =
		(v_x1_u32r -
		 (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * (int32_t)dig_H1) >>
		  4));

	if (v_x1_u32r < 0)
		v_x1_u32r = 0;
	if (v_x1_u32r > 419430400)
		v_x1_u32r = 419430400;

	return (uint32_t)(v_x1_u32r >> 12);
}
#ifdef __cplusplus
extern "C" {
#endif
static const char *TAG = "DS18B20_TEMPERATURE";

#define ONEWIRE_BUS_GPIO GPIO_NUM_4
void ds18B20_protocol_and_devcie_init();
float get_temperature();
#ifdef __cplusplus
}
#endif
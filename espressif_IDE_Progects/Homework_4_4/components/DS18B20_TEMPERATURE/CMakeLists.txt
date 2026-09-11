idf_component_register(SRCS "DS18B20_TEMPERATURE.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_adc "espressif__ds18b20" "espressif__onewire_bus")


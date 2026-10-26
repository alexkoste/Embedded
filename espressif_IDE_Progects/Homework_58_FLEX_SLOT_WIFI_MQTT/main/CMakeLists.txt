# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c         # list the source files of this component
    INCLUDE_DIRS  "."      # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES nvs_flash esp_wifi esp_event lwip mbedtls mqtt driver ssd1306 bme280 mqtt_manager wifi_manager          # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)

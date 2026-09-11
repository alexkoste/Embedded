# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c        # list the source files of this component
    INCLUDE_DIRS  "."      # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            # optional, list the public requirements (component names)
    	u8g2
    	driver
    	freertos
    	esp_adc
    	bme_280
    	i2c_bus
    	clock
    	DS18B20_TEMPERATURE
    	display

    PRIV_REQUIRES       # optional, list the private requirements
)

idf_component_register(SRCS "hello_world_main_battery.c" "hello_world_main.c"
                       PRIV_REQUIRES spi_flash esp_driver_gpio esp_adc
                       INCLUDE_DIRS "")

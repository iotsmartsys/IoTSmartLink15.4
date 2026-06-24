idf_component_register(SRCS "main.c" "iot154_radio.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES ieee802154 esp_timer esp_hw_support nvs_flash)

idf_component_register(SRCS "device_registry_nvs.c" "device_registry.c" "device_registry_policy.c" "main.c" "iot154_radio.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES ieee802154 esp_timer esp_hw_support nvs_flash esp_driver_uart)

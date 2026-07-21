idf_component_register(SRCS "main.cpp"
                            "reset/factory_reset_service.cpp"
                            "reset/reset_button_monitor.cpp"
                            "radio/iot154_sensor_client.c"
                            "storage/iot154_storage.c"
                       INCLUDE_DIRS "."
                                    "config"
                                    "radio"
                                    "reset"
                                    "storage"
                       REQUIRES iot154 issp_core issp_behaviors issp_transport_154 nvs_flash esp_driver_gpio
                       PRIV_REQUIRES ieee802154 esp_timer esp_hw_support)

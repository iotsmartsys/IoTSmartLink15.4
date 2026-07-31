idf_build_get_property(issp_app_154_target IDF_TARGET)

set(srcs "src/smart_sys_app.cpp")
set(priv_requires "issp_core" "issp_behaviors")

# smart_sys_app_hardware.cpp wires the real IEEE 802.15.4 transport/network
# manager/device/executor and factory reset services, so it (and the
# private dependencies it needs) is only built for targets with that radio.
# Other targets still get the state-machine core above, which automated
# tests exercise via SmartSysApp::SetupHooks without any of this.
if(issp_app_154_target STREQUAL "esp32h2" OR issp_app_154_target STREQUAL "esp32c6")
    list(APPEND srcs
        "src/smart_sys_app_hardware.cpp"
        "src/reset/factory_reset_service.cpp"
        "src/reset/reset_button_monitor.cpp"
    )
    list(APPEND priv_requires
        "issp_transport_154"
        "nvs_flash"
        "esp_timer"
        "esp_hw_support"
    )
endif()

idf_component_register(
    SRCS
        ${srcs}
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES
        esp_driver_gpio
    PRIV_REQUIRES
        ${priv_requires}
)

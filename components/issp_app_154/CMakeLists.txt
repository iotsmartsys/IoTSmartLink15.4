idf_build_get_property(issp_app_154_target IDF_TARGET)

# Allowlist de targets do repositório (TESTEXEC-008). O guard de projeto em
# cmake/require_supported_target.cmake já reprova antes de project(); esta
# verificação protege o componente quando ele for consumido por um projeto que
# não o inclua.
if(NOT issp_app_154_target STREQUAL "esp32h2" AND
   NOT issp_app_154_target STREQUAL "esp32c6")
    message(FATAL_ERROR
        "issp_app_154 does not support IDF_TARGET '${issp_app_154_target}'; "
        "admitted targets are esp32h2 esp32c6")
endif()

# smart_sys_app_hardware.cpp constrói o transporte, network manager, device,
# executor de reports e serviços de reset reais, e define o construtor de
# produto de um argumento. Todo target físico admitido possui rádio
# IEEE 802.15.4, então essas fontes entram em todo build deste componente.
# smart_sys_app_deep_sleep.cpp é target-agnóstico como smart_sys_app.cpp:
# contém a política de deep sleep e alcança os objetos de runtime somente pelos
# SetupHooks.
set(srcs
    "src/smart_sys_app.cpp"
    "src/smart_sys_app_deep_sleep.cpp"
    "src/smart_sys_app_hardware.cpp"
    "src/reset/factory_reset_service.cpp"
    "src/reset/reset_button_monitor.cpp"
)
set(priv_requires
    "issp_core"
    "issp_behaviors"
    "issp_transport_154"
    "nvs_flash"
    "esp_timer"
    "esp_hw_support"
)

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

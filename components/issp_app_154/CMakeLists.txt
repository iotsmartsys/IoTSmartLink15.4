idf_build_get_property(issp_app_154_target IDF_TARGET)

# Allowlist de targets do repositório (TESTEXEC-008). O guard de projeto em
# cmake/require_supported_target.cmake já reprova antes de project(); esta
# verificação protege o componente quando ele for consumido por um projeto que
# não o inclua. `linux` é a exceção host de TESTEXEC-003: compila apenas a
# lógica pura abaixo, nunca firmware nem prova de compatibilidade física.
if(NOT issp_app_154_target STREQUAL "esp32h2" AND
   NOT issp_app_154_target STREQUAL "esp32c6" AND
   NOT issp_app_154_target STREQUAL "linux")
    message(FATAL_ERROR
        "issp_app_154 does not support IDF_TARGET '${issp_app_154_target}'; "
        "admitted targets are esp32h2 esp32c6 (plus the host-native exception "
        "'linux', which builds pure logic only)")
endif()

# Núcleo target-agnostic: validação de configuração e máquina de estados de
# setup(), exercida pelos testes automatizados através de
# SmartSysApp::SetupHooks, que substitui por fakes todo passo que tocaria
# hardware.
set(srcs "src/smart_sys_app.cpp")
set(priv_requires "issp_core" "issp_behaviors")

# smart_sys_app_hardware.cpp constrói o transporte, network manager, device,
# executor de reports e serviços de reset reais, e define o construtor de
# produto de um argumento. Todo target físico admitido possui rádio
# IEEE 802.15.4, então essas fontes entram sempre; só a exceção host as omite,
# porque lá não existe rádio, NVS nem GPIO para construir.
if(NOT issp_app_154_target STREQUAL "linux")
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

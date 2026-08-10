# Allowlist de targets físicos do repositório (TESTEXEC-008).
#
# Inclua entre `include($ENV{IDF_PATH}/tools/cmake/project.cmake)` e `project()`:
# nesse ponto `__target_init()` já resolveu o cache `IDF_TARGET` a partir do
# ambiente, do cache ou do sdkconfig, e nenhum binário foi gerado ainda, de modo
# que uma seleção inválida reprova a configuração sem produzir artefato.
#
# Cada projeto declara também seu vínculo exclusivo antes do include, porque
# estar na allowlist não torna H2 e C6 intercambiáveis entre alvos:
#
#     set(ISSP_TARGET_BINDING esp32h2)
#     include(${CMAKE_CURRENT_LIST_DIR}/../cmake/require_supported_target.cmake)

set(ISSP_SUPPORTED_TARGETS esp32h2 esp32c6)

if(NOT IDF_TARGET)
    message(FATAL_ERROR
        "IoTSmartLink15.4 requires an explicit IDF_TARGET; admitted targets are "
        "${ISSP_SUPPORTED_TARGETS}")
endif()

# Exceção host explícita de TESTEXEC-003: o alvo `linux` do ESP-IDF define
# CONFIG_IDF_TARGET="linux" e é admitido apenas como ambiente de execução de
# lógica pura. Não é firmware, não é placa e não sustenta nenhuma alegação de
# compatibilidade IEEE 802.15.4, por isso também não responde ao vínculo por
# alvo verificado adiante.
if(IDF_TARGET STREQUAL "linux")
    message(STATUS
        "IoTSmartLink15.4 host-native exception: IDF_TARGET 'linux' builds pure "
        "logic only; physical targets remain ${ISSP_SUPPORTED_TARGETS}")
    return()
endif()

if(NOT IDF_TARGET IN_LIST ISSP_SUPPORTED_TARGETS)
    message(FATAL_ERROR
        "IoTSmartLink15.4 does not support IDF_TARGET '${IDF_TARGET}'; admitted "
        "targets are ${ISSP_SUPPORTED_TARGETS}. Both carry an IEEE 802.15.4 radio; "
        "no other chip is a target of this repository, and host-native logic tests "
        "are a separate strategy that never builds firmware.")
endif()

if(NOT DEFINED ISSP_TARGET_BINDING)
    message(FATAL_ERROR
        "IoTSmartLink15.4 requires this project to declare ISSP_TARGET_BINDING "
        "before including require_supported_target.cmake; admitted targets are "
        "${ISSP_SUPPORTED_TARGETS}")
endif()

if(NOT IDF_TARGET STREQUAL ISSP_TARGET_BINDING)
    message(FATAL_ERROR
        "Project '${CMAKE_CURRENT_LIST_DIR}' is bound to IDF_TARGET "
        "'${ISSP_TARGET_BINDING}', but '${IDF_TARGET}' was selected. Admitted "
        "targets of this repository are ${ISSP_SUPPORTED_TARGETS}, each bound to "
        "its own set of projects.")
endif()

message(STATUS
    "IoTSmartLink15.4 target allowlist satisfied: ${IDF_TARGET} "
    "(admitted: ${ISSP_SUPPORTED_TARGETS})")

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(ISSP_TARGET_BINDING esp32h2)
include(${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/require_supported_target.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(smart_sys_app_test)

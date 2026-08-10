cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/require_idf_6_0_1.cmake)
set(ISSP_TARGET_BINDING esp32c6)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/require_supported_target.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(central_154)

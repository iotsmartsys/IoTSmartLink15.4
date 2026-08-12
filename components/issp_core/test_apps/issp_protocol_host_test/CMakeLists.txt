cmake_minimum_required(VERSION 3.16)
project(issp_protocol_host_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# issp_protocol.cpp depends only on <cstdint>/<cstddef>, so the client codec is
# genuinely host-native. IsspDevice is not linked here: it depends on FreeRTOS
# and belongs to the physical H2 test app.
add_executable(issp_protocol_host_test
    test_issp_protocol.cpp
    ../../src/issp_protocol.cpp
)
target_include_directories(issp_protocol_host_test PRIVATE ../../include)
target_compile_options(issp_protocol_host_test PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME issp_protocol_host_test COMMAND issp_protocol_host_test)

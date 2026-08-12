cmake_minimum_required(VERSION 3.16)
project(iot154_packet_host_test C)

# iot154_packet.h depends only on stdbool/stddef/stdint/string.h, so the
# coordinator codec is genuinely host-native.
add_executable(iot154_packet_host_test
    test_iot154_packet.c
)
target_include_directories(iot154_packet_host_test PRIVATE ../../main)
target_compile_options(iot154_packet_host_test PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME iot154_packet_host_test COMMAND iot154_packet_host_test)

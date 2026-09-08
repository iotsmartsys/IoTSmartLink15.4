cmake_minimum_required(VERSION 3.16)
project(report_value_host_test C)

add_executable(report_value_host_test test_report_value.c ../../main/report_value.c)
target_include_directories(report_value_host_test PRIVATE ../../main)
target_compile_options(report_value_host_test PRIVATE -Wall -Wextra -Werror)
enable_testing()
add_test(NAME report_value_host_test COMMAND report_value_host_test)

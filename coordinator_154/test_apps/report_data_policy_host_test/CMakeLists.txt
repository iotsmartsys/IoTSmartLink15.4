cmake_minimum_required(VERSION 3.16)
project(report_data_policy_host_test C)

# The same decision code that governs production is exercised here; only the
# effects (host event acceptance and radio ACK) are substituted.
add_executable(report_data_policy_host_test
    test_report_data_policy.c
    ../../main/report_data_policy.c
    ../../main/device_registry_policy.c
)
target_include_directories(report_data_policy_host_test
    PRIVATE ../device_registry_policy_host_test/stubs ../../main)
target_compile_options(report_data_policy_host_test PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME report_data_policy_host_test COMMAND report_data_policy_host_test)

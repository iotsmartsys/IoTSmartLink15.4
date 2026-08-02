cmake_minimum_required(VERSION 3.16)
project(device_registry_policy_host_test C)

add_executable(device_registry_policy_host_test
    test_device_registry_policy.c
    ../../main/device_registry_policy.c
)
target_include_directories(device_registry_policy_host_test PRIVATE stubs ../../main)
target_compile_options(device_registry_policy_host_test PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME device_registry_policy_host_test COMMAND device_registry_policy_host_test)

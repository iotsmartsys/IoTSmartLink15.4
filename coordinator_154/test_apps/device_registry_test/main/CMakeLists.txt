idf_component_register(
    SRCS "test_device_registry.c" "../../../main/device_registry.c" "../../../main/device_registry_nvs.c" "../../../main/device_registry_policy.c"
    INCLUDE_DIRS "." "../../../main"
    REQUIRES unity nvs_flash
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE DEVICE_REGISTRY_NVS_TESTING)

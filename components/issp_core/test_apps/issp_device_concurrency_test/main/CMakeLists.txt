idf_component_register(
    SRCS "test_issp_device_concurrency.cpp"
    INCLUDE_DIRS "."
    REQUIRES issp_core unity freertos
)

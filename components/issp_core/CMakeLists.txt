idf_component_register(
    SRCS
        "src/issp_device.cpp"
        "src/issp_protocol.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos
)

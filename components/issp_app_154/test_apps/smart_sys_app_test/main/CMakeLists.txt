idf_component_register(
    SRCS "test_smart_sys_app.cpp"
    INCLUDE_DIRS "."
    REQUIRES issp_app_154 issp_core unity
    # The deep-sleep cases time the awake window with esp_timer.
    PRIV_REQUIRES esp_timer
)

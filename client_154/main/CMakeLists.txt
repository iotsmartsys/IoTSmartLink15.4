# The "IoTSmartLink15.4" menu (Kconfig.projbuild) chooses exactly one product
# firmware and one board model; this file is the only place that translates
# that choice into sources. Nothing else in the project reads these symbols,
# and no variant is compiled to be selected at runtime.
set(srcs "app_main.cpp")

# The menu itself is declared by this component, so its CONFIG_ symbols only
# exist after the early requirements-expansion pass. Sources are irrelevant in
# that pass, which is why the selection runs afterwards.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    if(CONFIG_IOTSMARTLINK154_PRODUCT_SINGLE_SMART_PLUG)
        list(APPEND srcs "firmwares/single_smart_plug.cpp")
        set(selected_product "Single smart plug")
        set(required_resources digital_output user_button)
    elseif(CONFIG_IOTSMARTLINK154_PRODUCT_DOOR_SENSOR)
        list(APPEND srcs "firmwares/door_sensor.cpp")
        set(selected_product "Door sensor")
        set(required_resources dry_contact_input user_button)
    else()
        message(FATAL_ERROR
            "No product firmware selected in the IoTSmartLink15.4 menu. "
            "Choose one under 'IoTSmartLink15.4 > Product firmware'.")
    endif()

    if(CONFIG_IOTSMARTLINK154_BOARD_CURRENT_CLIENT_ESP32H2_WIRING)
        list(APPEND srcs "boards/current_client_esp32h2_wiring.cpp")
        set(selected_board "Current client ESP32-H2 wiring")
        set(offered_resources digital_output user_button)
    elseif(CONFIG_IOTSMARTLINK154_BOARD_DOOR_SENSOR_BATTERY_H2)
        list(APPEND srcs "boards/door_sensor_battery_h2.cpp")
        set(selected_board "Door Sensor Battery H2")
        set(offered_resources dry_contact_input user_button)
    else()
        message(FATAL_ERROR
            "No board model selected in the IoTSmartLink15.4 menu for "
            "IDF_TARGET=${IDF_TARGET}. 'Current client ESP32-H2 wiring' and "
            "'Door Sensor Battery H2' are compatible with "
            "esp32h2 only; run 'idf.py set-target esp32h2' or add a board "
            "model for this target.")
    endif()

    foreach(required_resource IN LISTS required_resources)
        if(NOT required_resource IN_LIST offered_resources)
            message(FATAL_ERROR
                "Incompatible IoTSmartLink15.4 composition: product "
                "'${selected_product}' with board '${selected_board}' is "
                "missing physical resource '${required_resource}'.")
        endif()
    endforeach()
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES issp_app_154)

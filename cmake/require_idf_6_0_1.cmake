if(NOT IDF_VERSION_MAJOR EQUAL 6 OR
   NOT IDF_VERSION_MINOR EQUAL 0 OR
   NOT IDF_VERSION_PATCH EQUAL 1)
    message(FATAL_ERROR
        "IoTSmartLink15.4 requires ESP-IDF 6.0.1 exactly; found "
        "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH} at ${IDF_PATH}")
endif()

message(STATUS "IoTSmartLink15.4 ESP-IDF version locked to 6.0.1 (${IDF_PATH})")

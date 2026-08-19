set(registry_file "${CMAKE_CURRENT_LIST_DIR}/../../docs/adr/ADR-0005-CAPABILITY-IDENTITY.md")
set(packet_file "${CMAKE_CURRENT_LIST_DIR}/iot154_packet.h")
file(READ "${registry_file}" registry)
file(READ "${packet_file}" packet)

foreach(entry IN ITEMS "1|IOT154_EVENT_DOOR" "2|IOT154_EVENT_POWER"
                       "3|IOT154_EVENT_BATTERY_LEVEL_PERCENT"
                       "4|IOT154_EVENT_BATTERY_TELEMETRY_STATE")
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 type)
    list(GET parts 1 macro)
    if(NOT registry MATCHES "\\|[ \\t]*${type}[ \\t]*\\|")
        message(FATAL_ERROR "event registry mismatch: ADR-0005 has no type ${type}")
    endif()
    string(FIND "${packet}" "#define ${macro} ${type}" definition_offset)
    if(definition_offset EQUAL -1)
        message(FATAL_ERROR "event registry mismatch: ${macro} must be ${type}")
    endif()
endforeach()

set(registry_file "${CMAKE_CURRENT_LIST_DIR}/../../docs/adr/ADR-0005-CAPABILITY-IDENTITY.md")
set(packet_file "${CMAKE_CURRENT_LIST_DIR}/iot154_packet.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${registry_file}" "${packet_file}")
file(READ "${registry_file}" registry)
file(READ "${packet_file}" packet)

string(REGEX MATCHALL "\n\\|[ \t]*[0-9]+[ \t]*\\|" registry_types "${registry}")
list(LENGTH registry_types registry_type_count)
if(NOT registry_type_count EQUAL 4)
    message(FATAL_ERROR
        "event registry mismatch: ADR-0005 allocated type set must be exactly 1,2,3,4; "
        "found ${registry_type_count} numeric entries")
endif()

string(FIND "${packet}" "EVENT_REGISTRY_BEGIN" registry_begin)
string(FIND "${packet}" "EVENT_REGISTRY_END" registry_end)
if(registry_begin EQUAL -1 OR registry_end EQUAL -1 OR
   registry_end LESS_EQUAL registry_begin)
    message(FATAL_ERROR
        "event registry mismatch: coordinator registry markers are missing or invalid")
endif()
math(EXPR packet_registry_length "${registry_end} - ${registry_begin}")
string(SUBSTRING "${packet}" ${registry_begin} ${packet_registry_length}
       packet_registry)
string(REGEX MATCHALL
       "#define[ \t]+IOT154_EVENT_[A-Z0-9_]+[ \t]+[0-9]+"
       packet_types "${packet_registry}")
list(LENGTH packet_types packet_type_count)
if(NOT packet_type_count EQUAL 4)
    message(FATAL_ERROR
        "event registry mismatch: coordinator allocated type set must be exactly 1,2,3,4; "
        "found ${packet_type_count} IOT154_EVENT definitions: ${packet_types}")
endif()

set(expected_types 1 2 3 4)
set(registry_capabilities
    "Sensor de porta"
    "Plug comutável"
    "Nível de bateria em percentual"
    "Estado da telemetria de bateria")
set(packet_macros
    IOT154_EVENT_DOOR
    IOT154_EVENT_POWER
    IOT154_EVENT_BATTERY_LEVEL_PERCENT
    IOT154_EVENT_BATTERY_TELEMETRY_STATE)

foreach(index RANGE 0 3)
    list(GET expected_types ${index} expected_type)
    list(GET registry_capabilities ${index} capability)
    list(GET packet_macros ${index} macro)

    string(REGEX MATCH
           "\n\\|[ \\t]*([0-9]+)[ \\t]*\\|[ \\t]*${capability}[ \\t]*\\|"
           capability_row "${registry}")
    if(capability_row STREQUAL "")
        message(FATAL_ERROR
            "event registry mismatch: ADR-0005 has no allocation for '${capability}'")
    endif()
    set(registry_type "${CMAKE_MATCH_1}")
    if(NOT registry_type STREQUAL expected_type)
        message(FATAL_ERROR
            "event registry mismatch: ADR-0005 allocates '${capability}' as type "
            "${registry_type}; expected ${expected_type}")
    endif()

    string(REGEX MATCH
           "#define[ \\t]+${macro}[ \\t]+([0-9]+)"
           packet_definition "${packet_registry}")
    if(packet_definition STREQUAL "")
        message(FATAL_ERROR
            "event registry mismatch: coordinator has no definition for ${macro}")
    endif()
    set(packet_type "${CMAKE_MATCH_1}")
    if(NOT packet_type STREQUAL registry_type)
        message(FATAL_ERROR
            "event registry mismatch: ADR-0005 allocates '${capability}' as type "
            "${registry_type}, but ${macro} is ${packet_type}")
    endif()
endforeach()

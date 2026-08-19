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

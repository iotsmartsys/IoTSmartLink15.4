#include "report_value.h"

#include <float.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "iot154_packet.h"

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
               FLT_MAX_EXP == 128 && DBL_MANT_DIG >= 53, "ISSP requires IEEE 754 binary32");

bool report_value_format(uint8_t event_type, uint8_t value_type, uint32_t bits,
                         char *output, size_t capacity)
{
    if (output == NULL || !iot154_value_is_canonical(value_type, bits)) {
        return false;
    }

    const int32_t integer = bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)~bits;
    float fractional = 0;
    if (value_type == IOT154_VALUE_FLOAT32) {
        memcpy(&fractional, &bits, sizeof(fractional));
    }
    const bool is_integer = value_type == IOT154_VALUE_INT32;
    const char *text = NULL;
    switch (event_type) {
    case IOT154_EVENT_DOOR:
        if (!is_integer || bits > 1) return false;
        text = bits == 1 ? "open" : "closed";
        break;
    case IOT154_EVENT_PRESENCE:
        if (!is_integer || bits > 1) return false;
        text = bits == 1 ? "detected" : "undetected";
        break;
    case IOT154_EVENT_POWER:
        if (!is_integer || bits > 2) return false;
        text = bits == 2 ? "toggle" : (bits == 1 ? "on" : "off");
        break;
    case IOT154_EVENT_BATTERY_TELEMETRY_STATE:
        if (!is_integer || bits > 2) return false;
        text = bits == 2 ? "inert" : (bits == 1 ? "approximate" : "calibrated");
        break;
    case IOT154_EVENT_BATTERY_LEVEL_PERCENT:
    case IOT154_EVENT_LIGHT_PERCENT:
        if (is_integer ? (integer < 0 || integer > 100)
                       : (fractional < 0 || fractional > 100)) return false;
        break;
    default:
        break;
    }

    char formatted[REPORT_VALUE_TEXT_SIZE];
    if (text == NULL) {
        int length;
        if (is_integer) {
            length = snprintf(formatted, sizeof(formatted), "%" PRId32, integer);
        } else {
            const bool negative = (bits & 0x80000000U) != 0;
            const double magnitude = negative ? -(double)fractional : (double)fractional;
            if (magnitude >= 8388608.0) {
                // Every binary32 at this magnitude is integral. Avoid scaling
                // or integer casts which would overflow near FLT_MAX.
                length = snprintf(formatted, sizeof(formatted), "%s%.0f.00",
                                  negative ? "-" : "", magnitude);
            } else {
                // binary32 * 100 is exact in binary64 here. Round half away
                // from zero, then print integer parts without locale decimals.
                const uint32_t cents = (uint32_t)(magnitude * 100.0 + 0.5);
                length = snprintf(formatted, sizeof(formatted), "%s%" PRIu32 ".%02" PRIu32,
                                  negative && cents != 0 ? "-" : "", cents / 100, cents % 100);
            }
        }
        if (length < 0 || (size_t)length >= sizeof(formatted)) return false;
        text = formatted;
    }
    const size_t length = strlen(text);
    if (capacity <= length) return false;
    memcpy(output, text, length + 1);
    return true;
}

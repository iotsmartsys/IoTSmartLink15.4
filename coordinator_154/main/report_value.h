#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Covers sign, all 39 integer digits of binary32, decimal point, cents and NUL.
#define REPORT_VALUE_TEXT_SIZE 48

bool report_value_format(uint8_t event_type, uint8_t value_type, uint32_t bits,
                         char *output, size_t capacity);

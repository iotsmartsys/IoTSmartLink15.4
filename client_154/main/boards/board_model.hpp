#pragma once

#include "driver/gpio.h"

namespace client154
{

// Electrical description of a concrete board. It carries no product rule,
// identity, endpoint, timing or protocol detail: a product firmware reads
// these values instead of writing GPIO numbers of its own.
struct BoardModel
{
    gpio_num_t relayPin;
    bool relayActiveHigh;
    gpio_num_t factoryResetButtonPin;
    bool factoryResetButtonActiveLow;
};

// Exactly one file under boards/ defines this function: the one chosen in
// "IoTSmartLink15.4 > Board model" and selected by main/CMakeLists.txt.
const BoardModel &selectedBoard();

} // namespace client154

#pragma once

#include "SmartSysApp.h"

namespace client154
{

// Contract between the entrypoint and the selected product firmware.
//
// Exactly one file under firmwares/ defines this function: the one chosen in
// "App Client > Product firmware" and selected by main/CMakeLists.txt.
// The definition owns the device identity, the capabilities and the product
// rules; it reads pins and polarity from the selected board model and returns
// the result of SmartSysApp::setup() so the entrypoint can report it.
iotsmartsys::SetupResult startSelectedProductFirmware();

} // namespace client154

#pragma once
// =============================================================================
//  bpad_hal.h - bpad Hardware Abstraction Layer & Driver Suite
// =============================================================================
//  Targets bpad 185 and next-generation bpad hardware on ESP32 Core 3.x+ / IDF 5.x.
//
//  Usage:
//    #include <bpad_hal.h>
//    const bpadConfig hw = bpadConfig::defaultBoard(); // or bpad185()
//
//  Unified driver classes:
//    bpadDisplay, bpadTouch, bpadIMU, bpadCompass, bpadAudio, bpadHaptics, bpadStorage
// =============================================================================

#include "bpad_config.h"
#include "bpad_display.h"
#include "bpad_touch.h"
#include "bpad_imu.h"
#include "bpad_compass.h"
#include "bpad_audio.h"
#include "bpad_haptics.h"
#include "bpad_storage.h"
#include "bpad_power.h"

#pragma once

#define LV_USE_DRAW_SW 1
#define LV_USE_TFT_ESPI 1

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

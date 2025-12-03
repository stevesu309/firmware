#pragma once
#include "OLEDDisplay.h"
#include <stdint.h>

typedef struct
{
    const char *utf8;
    const uint8_t bitmap[32];
} ChineseFont;

extern const ChineseFont chineseFont[];
extern const unsigned int chineseFontCount;

bool drawChineseChar(OLEDDisplay *display, int16_t x, int16_t y, const char *utf8);
void drawChineseStringWithLineBreak(OLEDDisplay *display, int16_t x, int16_t y, const char *str);

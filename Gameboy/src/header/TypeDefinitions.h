#pragma once

#include <stdint.h>

typedef uint8_t		BYTE;
typedef int8_t		SIGNED_BYTE;
typedef uint16_t	WORD;
typedef uint32_t	MEMORY_ADDRESS;

static constexpr BYTE BIT_0 = (1 << 0);
static constexpr BYTE BIT_1 = (1 << 1);
static constexpr BYTE BIT_2 = (1 << 2);
static constexpr BYTE BIT_3 = (1 << 3);
static constexpr BYTE BIT_4 = (1 << 4);
static constexpr BYTE BIT_5 = (1 << 5);
static constexpr BYTE BIT_6 = (1 << 6);
static constexpr BYTE BIT_7 = (1 << 7);

static constexpr int SCREEN_WIDTH  = 160;
static constexpr int SCREEN_HEIGHT = 144;
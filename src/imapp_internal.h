#pragma once

#include "imapp/imapp.h"

#include "imapp_defines.h"

#include "imui/../../src/imui_internal.h"
#include "imui/../../src/imui_memory.h"
#include "imui/../../src/imui_helpers.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct ImAppPlatform ImAppPlatform;
typedef struct ImAppRenderer ImAppRenderer;
typedef struct ImAppResSys ImAppResSys;
typedef struct ImAppWindow ImAppWindow;
typedef struct ImAppFont ImAppFont;

struct ImAppContext
{
	ImUiAllocator			allocator;

	bool					running;
	bool					isFullscrene;
	bool					useWindowStyle;
	int						exitCode;
	int64_t					tickIntervalMs;
	int64_t					lastTickValue;
	ImUiInputMouseCursor	lastCursor;

	ImAppPlatform*			platform;
	ImUiContext*			imui;
	ImAppRenderer*			renderer;
	ImAppResSys*			ressys;

	ImAppWindow**			windows;
	uintsize				windowsCount;
	uintsize				windowsCapacity;

	ImAppResPak*			defaultResPak;
	ImAppFont*				defaultFont;

	void*					programContext;
	ImUiFrame*				frame;
};

#pragma once

#include "imapp/imapp.h"

#include "imapp_defines.h"
#include "imapp_renderer.h"

#include "imui/../../src/imui_internal.h"
#include "imui/../../src/imui_memory.h"
#include "imui/../../src/imui_helpers.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct ImAppFont ImAppFont;
typedef struct ImAppPlatform ImAppPlatform;
typedef struct ImAppRenderer ImAppRenderer;
typedef struct ImAppRendererWindow ImAppRendererWindow;
typedef struct ImAppResSys ImAppResSys;
typedef struct ImAppWindow ImAppWindow;

typedef struct ImAppContextWindowInfo
{
	ImAppWindow*			window;

	ImAppWindowDoUiFunc		uiFunc;
	void*					uiContext;

	ImAppRendererWindow		rendererWindow;
	float					clearColor[ 4u ];

	bool					isRendererCreated;
	bool					isDestroyed;
} ImAppContextWindowInfo;

struct ImAppContext
{
	ImUiAllocator			allocator;

	bool					running;
	int						exitCode;
	int64_t					tickIntervalMs;
	int64_t					lastTickValue;
	ImUiInputMouseCursor	lastCursor;
	double					lastFocusExecuteTime;

	ImAppPlatform*			platform;
	ImUiContext*			imui;
	ImAppRenderer*			renderer;
	ImAppResSys*			ressys;

	ImAppContextWindowInfo*	windows;
	uintsize				windowsCount;
	uintsize				windowsCapacity;

	ImAppResPak*			defaultResPak;
	ImAppFont*				defaultFont;

	void*					programContext;
	ImUiFrame*				frame;
};

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
typedef struct ImAppRendererTexture ImAppRendererTexture;

typedef struct ImAppInternal
{
	ImAppContext			context;		// must be at offset 0

	ImUiAllocator			allocator;

	bool					running;
	uint8_t					inputMaskShift;
	uint8_t					inputMaskControl;
	uint8_t					inputMaskAlt;
	uint32_t				inputModifiers;
	uint32_t				inputDownMask;

	int64_t					tickIntervalMs;
	int64_t					lastTickValue;
	ImUiInputMouseCursor	lastCursor;

	ImAppPlatform*			platform;
	ImAppWindow*			window;
	ImAppRenderer*			renderer;
	ImAppResSys*			ressys;

	ImAppResPak*			defaultResPak;
	ImUiFont*				defaultFont;
	ImAppRendererTexture*	defaultFontTexture;

	void*					programContext;
} ImAppInternal;

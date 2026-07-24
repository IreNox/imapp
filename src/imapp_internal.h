#pragma once

#include "imapp/imapp.h"

#include "imapp_defines.h"
#include "imapp_renderer.h"

#include "imui/../../src/imui_internal.h"
#include "imui/../../src/imui_memory.h"
#include "imui/../../src/imui_helpers.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct ImappFont ImappFont;
typedef struct ImappPlatform ImappPlatform;
typedef struct ImappRenderer ImappRenderer;
typedef struct ImappRendererWindow ImappRendererWindow;
typedef struct ImappResSys ImappResSys;
typedef struct ImappWindow ImappWindow;

typedef struct ImappContextWindowInfo
{
	ImappWindow*			window;
	const ImuiInputState*	inputState;

	ImappWindowDoUiFunc		uiFunc;
	void*					uiContext;

	ImappRendererWindow		rendererWindow;
	float					clearColor[ 4u ];

	bool					isRendererCreated;
	bool					isDestroyed;
} ImappContextWindowInfo;

struct ImappContext
{
	ImuiAllocator			allocator;

	bool					running;
	int						exitCode;
	int64_t					tickIntervalMs;
	int64_t					lastTickValue;
	ImuiInputMouseCursor	lastCursor;
	double					lastFocusExecuteTime;

	ImappPlatform*			platform;
	ImuiContext*			imui;
	ImappRenderer*			renderer;
	ImappResSys*			ressys;

	ImappContextWindowInfo*	windows;
	uintsize				windowsCount;
	uintsize				windowsCapacity;

	ImappResPak*			defaultResPak;
	ImappFont*				defaultFont;

	void*					programContext;
	ImuiFrame*				frame;
};

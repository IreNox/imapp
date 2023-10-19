#pragma once

typedef enum ImAppEventType ImAppEventType;
enum ImAppEventType
{
	// Window
	ImAppEventType_WindowClose,

	// Input
	ImAppEventType_KeyDown,
	ImAppEventType_KeyUp,
	ImAppEventType_Character,
	ImAppEventType_Motion,
	ImAppEventType_ButtonDown,
	ImAppEventType_ButtonUp,
	ImAppEventType_Scroll
};

typedef struct ImAppWindowEvent ImAppWindowEvent;
struct ImAppWindowEvent
{
	ImAppEventType			type;
};

typedef struct ImAppInputKeyEvent ImAppInputKeyEvent;
struct ImAppInputKeyEvent
{
	ImAppEventType			type;
	ImUiInputKey			key;
	bool					repeat;
};

typedef struct ImAppInputCharacterEvent ImAppInputCharacterEvent;
struct ImAppInputCharacterEvent
{
	ImAppEventType			type;
	char					character;
};

typedef struct ImAppInputMotionEvent ImAppInputMotionEvent;
struct ImAppInputMotionEvent
{
	ImAppEventType			type;
	int32_t					x;
	int32_t					y;
};

typedef struct ImAppInputButtonEvent ImAppInputButtonEvent;
struct ImAppInputButtonEvent
{
	ImAppEventType			type;
	int32_t					x;
	int32_t					y;
	ImUiInputMouseButton	button;
	uint8_t					repeateCount;
};

typedef struct ImAppInputScrollEvent ImAppInputScrollEvent;
struct ImAppInputScrollEvent
{
	ImAppEventType			type;
	int32_t					x;
	int32_t					y;
};

typedef union ImAppEvent ImAppEvent;
union ImAppEvent
{
	ImAppEventType				type;

	ImAppWindowEvent			window;
	ImAppInputKeyEvent			key;
	ImAppInputCharacterEvent	character;
	ImAppInputMotionEvent		motion;
	ImAppInputButtonEvent		button;
	ImAppInputScrollEvent		scroll;
};

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
	ImAppEventType_DoubleClick,
	ImAppEventType_Scroll,
	ImAppEventType_Direction
};

typedef struct ImAppWindowEvent
{
	ImAppEventType			type;
} ImAppWindowEvent;

typedef struct ImAppInputKeyEvent
{
	ImAppEventType			type;
	ImUiInputKey			key;
	bool					repeat;
} ImAppInputKeyEvent;

typedef struct ImAppInputCharacterEvent
{
	ImAppEventType			type;
	uint32_t				character;
} ImAppInputCharacterEvent;

typedef struct ImAppInputMotionEvent
{
	ImAppEventType			type;
	int32_t					x;
	int32_t					y;
} ImAppInputMotionEvent;

typedef struct ImAppInputButtonEvent
{
	ImAppEventType			type;
	ImUiInputMouseButton	button;
	uint8_t					repeateCount;
} ImAppInputButtonEvent;

typedef struct ImAppInputScrollEvent
{
	ImAppEventType			type;
	int32_t					x;
	int32_t					y;
} ImAppInputScrollEvent;

typedef struct ImAppInputDirectionEvent
{
	ImAppEventType			type;
	float					x;
	float					y;
} ImAppInputDirectionEvent;

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
	ImAppInputDirectionEvent	direction;
};

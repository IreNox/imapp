#pragma once

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
typedef enum ImAppEventType ImAppEventType;

struct ImAppWindowEvent
{
	ImAppEventType			type;
};
typedef struct ImAppWindowEvent ImAppWindowEvent;

struct ImAppInputKeyEvent
{
	ImAppEventType			type;
	ImAppInputKey			key;
};
typedef struct ImAppInputKeyEvent ImAppInputKeyEvent;

struct ImAppInputCharacterEvent
{
	ImAppEventType			type;
	char					character;
};
typedef struct ImAppInputCharacterEvent ImAppInputCharacterEvent;

struct ImAppInputMotionEvent
{
	ImAppEventType			type;
	int32_t					x;
	int32_t					y;
};
typedef struct ImAppInputMotionEvent ImAppInputMotionEvent;

enum ImAppInputButton
{
	ImAppInputButton_Left,
	ImAppInputButton_Right,
	ImAppInputButton_Middle,
	ImAppInputButton_X1,
	ImAppInputButton_X2
};
typedef enum ImAppInputButton ImAppInputButton;

struct ImAppInputButtonEvent
{
	ImAppEventType			type;
	int32_t					x;
	int32_t					y;
	ImAppInputButton		button;
	uint8_t					repeateCount;
};
typedef struct ImAppInputButtonEvent ImAppInputButtonEvent;

struct ImAppInputScrollEvent
{
	ImAppEventType			type;
	int32_t					x;
	int32_t					y;
};
typedef struct ImAppInputScrollEvent ImAppInputScrollEvent;

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
typedef union ImAppEvent ImAppEvent;

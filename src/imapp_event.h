#pragma once

typedef enum ImappEventType ImappEventType;
enum ImappEventType
{
	// Window
	ImappEventType_WindowClose,

	// Input
	ImappEventType_KeyDown,
	ImappEventType_KeyUp,
	ImappEventType_Character,
	ImappEventType_Motion,
	ImappEventType_ButtonDown,
	ImappEventType_ButtonUp,
	ImappEventType_DoubleClick,
	ImappEventType_Scroll,
	ImappEventType_Direction
};

typedef struct ImappWindowEvent
{
	ImappEventType			type;
} ImappWindowEvent;

typedef struct ImappInputKeyEvent
{
	ImappEventType			type;
	ImuiInputKey			key;
	bool					repeat;
} ImappInputKeyEvent;

typedef struct ImappInputCharacterEvent
{
	ImappEventType			type;
	uint32_t				character;
} ImappInputCharacterEvent;

typedef struct ImappInputMotionEvent
{
	ImappEventType			type;
	int32_t					x;
	int32_t					y;
} ImappInputMotionEvent;

typedef struct ImappInputButtonEvent
{
	ImappEventType			type;
	ImuiInputMouseButton	button;
	uint8_t					repeateCount;
} ImappInputButtonEvent;

typedef struct ImappInputScrollEvent
{
	ImappEventType			type;
	int32_t					x;
	int32_t					y;
} ImappInputScrollEvent;

typedef struct ImappInputDirectionEvent
{
	ImappEventType			type;
	float					x;
	float					y;
} ImappInputDirectionEvent;

typedef union ImappEvent
{
	ImappEventType				type;

	ImappWindowEvent			window;
	ImappInputKeyEvent			key;
	ImappInputCharacterEvent	character;
	ImappInputMotionEvent		motion;
	ImappInputButtonEvent		button;
	ImappInputScrollEvent		scroll;
	ImappInputDirectionEvent	direction;
} ImappEvent;

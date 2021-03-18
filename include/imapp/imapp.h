#pragma once

#include <stdbool.h>
#include <stdint.h>

#if defined(__ANDROID__)
#	define NK_SIZE_TYPE size_t
#	define NK_POINTER_TYPE size_t
#endif

#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_BUTTON_TRIGGER_ON_RELEASE
#include "nuklear.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ImAppParameters ImAppParameters;
typedef struct ImAppContext ImAppContext;
typedef struct ImAppImage ImAppImage;

typedef uint32_t ImAppColor;

typedef void*(*ImAppAllocatorMallocFunc)(size_t size, void* userData);
typedef void(*ImAppAllocatorFreeFunc)(void* memory, void* userData);

//////////////////////////////////////////////////////////////////////////
// These function must be implemented to create a ImApp Program:

// Called at startup to initialize Program and create Context. To customize the App change values in pParameters. Must return a Program Context. Return NULL to signal an Error.
void*						ImAppProgramInitialize( ImAppParameters* parameters );

// Called for every tick to build the UI
void						ImAppProgramDoUi( ImAppContext* imAppContext, void* programContext );

// Called before shutdown. Free the Program Context here.
void						ImAppProgramShutdown( ImAppContext* imAppContext, void* programContext );

//////////////////////////////////////////////////////////////////////////
// Control

void						ImAppQuit( ImAppContext* imAppContext );

//////////////////////////////////////////////////////////////////////////
// Image

// Blocking Image 
ImAppImage*					ImAppImageLoadResource( ImAppContext* imAppContext, const char* resourceName );
ImAppImage*					ImAppImageLoadFromMemory( ImAppContext* imAppContext, const void* imageData, size_t imageDataSize, int width, int height );
void						ImAppImageFree( ImAppContext* imAppContext, ImAppImage* image );

struct nk_image				ImAppImageNuklear( ImAppImage* image );

// Loads async and returns the Image Resource. While the Image is loading returns an default image with given attributes.
struct nk_image				ImAppImageGet( ImAppContext* imAppContext, const char* resourceName, int defaultWidth, int defaultHeight, ImAppColor defaultColor );

// Loads blocking and returns the given Image Resource.
struct nk_image				ImAppImageGetBlocking( ImAppContext* imAppContext, const char* resourceName );

//////////////////////////////////////////////////////////////////////////
// Color

uint8_t						ImAppColorGetR( ImAppColor color );
uint8_t						ImAppColorGetG( ImAppColor color );
uint8_t						ImAppColorGetB( ImAppColor color );
uint8_t						ImAppColorGetA( ImAppColor color );

ImAppColor					ImAppColorSetRGB( uint8_t r, uint8_t g, uint8_t b );
ImAppColor					ImAppColorSetRGBA( uint8_t r, uint8_t g, uint8_t b, uint8_t a );

ImAppColor					ImAppColorSetFloatRGB( float r, float g, float b );				// Floats between 0.0 and 1.0
ImAppColor					ImAppColorSetFloatRGBA( float r, float g, float b, float a );	// Floats between 0.0 and 1.0

//////////////////////////////////////////////////////////////////////////
// Input

enum ImAppInputKey
{
	ImAppInputKey_None,

	ImAppInputKey_A,
	ImAppInputKey_B,
	ImAppInputKey_C,
	ImAppInputKey_D,
	ImAppInputKey_E,
	ImAppInputKey_F,
	ImAppInputKey_G,
	ImAppInputKey_H,
	ImAppInputKey_I,
	ImAppInputKey_J,
	ImAppInputKey_K,
	ImAppInputKey_L,
	ImAppInputKey_M,
	ImAppInputKey_N,
	ImAppInputKey_O,
	ImAppInputKey_P,
	ImAppInputKey_Q,
	ImAppInputKey_R,
	ImAppInputKey_S,
	ImAppInputKey_T,
	ImAppInputKey_U,
	ImAppInputKey_V,
	ImAppInputKey_W,
	ImAppInputKey_X,
	ImAppInputKey_Y,
	ImAppInputKey_Z,
	
	ImAppInputKey_1,
	ImAppInputKey_2,
	ImAppInputKey_3,
	ImAppInputKey_4,
	ImAppInputKey_5,
	ImAppInputKey_6,
	ImAppInputKey_7,
	ImAppInputKey_8,
	ImAppInputKey_9,
	ImAppInputKey_0,

	ImAppInputKey_Enter,
	ImAppInputKey_Escape,
	ImAppInputKey_Backspace,
	ImAppInputKey_Tab,
	ImAppInputKey_Space,

	ImAppInputKey_LeftShift,
	ImAppInputKey_RightShift,
	ImAppInputKey_LeftControl,
	ImAppInputKey_RightControl,
	ImAppInputKey_LeftAlt,
	ImAppInputKey_RightAlt,

	ImAppInputKey_Minus,
	ImAppInputKey_Equals,
	ImAppInputKey_LeftBracket,
	ImAppInputKey_RightBracket,
	ImAppInputKey_Backslash,
	ImAppInputKey_Semicolon,
	ImAppInputKey_Apostrophe,
	ImAppInputKey_Grave,
	ImAppInputKey_Comma,
	ImAppInputKey_Period,
	ImAppInputKey_Slash,

	ImAppInputKey_F1,
	ImAppInputKey_F2,
	ImAppInputKey_F3,
	ImAppInputKey_F4,
	ImAppInputKey_F5,
	ImAppInputKey_F6,
	ImAppInputKey_F7,
	ImAppInputKey_F8,
	ImAppInputKey_F9,
	ImAppInputKey_F10,
	ImAppInputKey_F11,
	ImAppInputKey_F12,

	ImAppInputKey_Print,
	ImAppInputKey_Pause,

	ImAppInputKey_Insert,
	ImAppInputKey_Delete,
	ImAppInputKey_Home,
	ImAppInputKey_End,
	ImAppInputKey_PageUp,
	ImAppInputKey_PageDown,

	ImAppInputKey_Up,
	ImAppInputKey_Left,
	ImAppInputKey_Down,
	ImAppInputKey_Right,

	ImAppInputKey_Numpad_Divide,
	ImAppInputKey_Numpad_Multiply,
	ImAppInputKey_Numpad_Minus,
	ImAppInputKey_Numpad_Plus,
	ImAppInputKey_Numpad_Enter,
	ImAppInputKey_Numpad_1,
	ImAppInputKey_Numpad_2,
	ImAppInputKey_Numpad_3,
	ImAppInputKey_Numpad_4,
	ImAppInputKey_Numpad_5,
	ImAppInputKey_Numpad_6,
	ImAppInputKey_Numpad_7,
	ImAppInputKey_Numpad_8,
	ImAppInputKey_Numpad_9,
	ImAppInputKey_Numpad_0,
	ImAppInputKey_Numpad_Period,

	ImAppInputKey_MAX
};
typedef enum ImAppInputKey ImAppInputKey;

enum ImAppInputModifier
{
	ImAppInputModifier_Shift	= 1u << 0u,
	ImAppInputModifier_Ctrl		= 1u << 1u,
	ImAppInputModifier_Alt		= 1u << 2u
};

struct ImAppInputShortcut
{
	unsigned		modifierMask;	// Flags of ImAppInputModifier
	ImAppInputKey	key;
	enum nk_keys	nkKey;
};
typedef struct ImAppInputShortcut ImAppInputShortcut;

//////////////////////////////////////////////////////////////////////////
// Types

struct ImAppAllocator
{
	ImAppAllocatorMallocFunc	mallocFunc;
	ImAppAllocatorFreeFunc		freeFunc;
	void*						userData;
};
typedef struct ImAppAllocator ImAppAllocator;

struct ImAppParameters
{
	int							argc;
	char**						argv;

	ImAppAllocator				allocator;			// Override memory Allocator. Default use malloc/free

	int							tickIntervalMs;		// Tick interval. Use 0 to disable. Default: 0
	bool						defaultFullWindow;	// Opens a default Window over the full size. Default: true


	const ImAppInputShortcut*	shortcuts;
	size_t						shortcutsLength;

	// Only for windowed Platforms:
	const char*					windowTitle;		// Default: "I'm App"
	int							windowWidth;		// Default: 1280
	int							windowHeight;		// Default: 720
};

struct ImAppContext
{
	struct nk_context*			nkContext;

	int							x;
	int							y;
	int							width;
	int							height;
};

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "imui/imui.h"
#include "imui/imui_toolbox.h"

#ifdef __cplusplus
#	include "imui/imui_cpp.h"

extern "C"
{
#endif

typedef struct ImAppContext ImAppContext;
typedef struct ImAppImage ImAppImage;
typedef struct ImAppWindow ImAppWindow;

typedef struct ImAppParameters ImAppParameters;
struct ImAppParameters
{
	ImUiAllocator				allocator;			// Override memory Allocator. Default use malloc/free

	int							tickIntervalMs;		// Tick interval. Use 0 to disable. Default: 0

	const char*					resourcePath;		// Path where resources loaded from. use ./ for relative to executable. default: {exe_dir}/assets

	const char*					defaultFontName;	// default: arial.ttf;
	float						defaultFontSize;	// default: 16

	//const ImAppInputShortcut*	shortcuts;
	//size_t						shortcutsLength;

	// Only for windowed Platforms:
	bool						windowEnable;		// Opens a default Window. Default: true
	const char*					windowTitle;		// Default: "I'm App"
	int							windowWidth;		// Default: 1280
	int							windowHeight;		// Default: 720
};

typedef void*(*ImAppWindowDoUiFunc)( ImAppContext* imapp, void* programContext, ImUiSurface* surface );

//////////////////////////////////////////////////////////////////////////
// Program entry points
// These function must be implemented to create a ImApp Program:

// Called at startup to initialize Program and create Context. To customize the App change values in pParameters. Must return a Program Context. Return NULL to signal an Error.
void*						ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] );

// Called for every tick to build the UI.
void						ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImUiSurface* surface );

// Called before shutdown. Free the Program Context here.
void						ImAppProgramShutdown( ImAppContext* imapp, void* programContext );

//////////////////////////////////////////////////////////////////////////
// Control

// TODO:
// Create a Window at given coordinates. uiFunc callback will be called every frame to build UI.
//ImAppWindow*				ImAppWindowCreate( ImAppContext* imapp, ImUiStringView title, uint32_t x, uint32_t y, uint32_t width, uint32_t height, ImAppWindowDoUiFunc uiFunc );
//void						ImAppWindowDestroy( ImAppWindow* window );

void						ImAppQuit( ImAppContext* imapp );

//////////////////////////////////////////////////////////////////////////
// Resources

// Blocking Image
ImAppImage*					ImAppImageLoadResource( ImAppContext* imapp, ImUiStringView resourceName );
ImAppImage*					ImAppImageCreateRaw( ImAppContext* imapp, const void* imageData, size_t imageDataSize, int width, int height );
ImAppImage*					ImAppImageCreatePng( ImAppContext* imapp, const void* imageData, size_t imageDataSize );
//ImAppImage*					ImAppImageCreateJpeg( ImAppContext* imapp, const void* imageData, size_t imageDataSize );
void						ImAppImageFree( ImAppContext* imapp, ImAppImage* image );

ImUiTexture					ImAppImageGetTexture( const ImAppImage* image );

// Loads async and returns the Image Resource. While the Image is loading returns an default image with given attributes.
ImUiTexture					ImAppImageGet( ImAppContext* imapp, ImUiStringView resourceName, int defaultWidth, int defaultHeight, ImUiColor defaultColor );

// Loads blocking and returns the given Image Resource.
ImUiTexture					ImAppImageGetBlocking( ImAppContext* imapp, ImUiStringView resourceName );

ImUiFont*					ImAppFontGet( ImAppContext* imapp, ImUiStringView fontName, float fontSize );

//////////////////////////////////////////////////////////////////////////
// Input

//struct ImAppInputShortcut
//{
//	unsigned		modifierMask;	// Flags of ImAppInputModifier
//	ImAppInputKey	key;
//	//enum nk_keys	nkKey;
//};
//typedef struct ImAppInputShortcut ImAppInputShortcut;

//////////////////////////////////////////////////////////////////////////
// Types

struct ImAppContext
{
	ImUiContext*				imui;

	int							x;
	int							y;
	int							width;
	int							height;
};

#ifdef __cplusplus
}
#endif

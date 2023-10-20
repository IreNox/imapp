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
typedef struct ImAppResPak ImAppResPak;

typedef struct ImAppParameters ImAppParameters;
struct ImAppParameters
{
	ImUiAllocator				allocator;			// Override memory Allocator. Default: malloc/free

	int							tickIntervalMs;		// Tick interval. Use 0 to disable. Default: 0

	const char*					resPath;			// Path where resources loaded from. use ./ for relative to executable. default: {exe_dir}/assets
	const char*					defaultResPak;

	const char*					defaultFontName;	// default: arial.ttf;
	float						defaultFontSize;	// default: 16

	//const ImAppInputShortcut*	shortcuts;
	//size_t						shortcutsLength;

	// Only for windowed Platforms:
	bool						windowEnable;		// Opens a default Window. Default: true
	const char*					windowTitle;		// Default: "I'm App"
	int							windowWidth;		// Default: 1280
	int							windowHeight;		// Default: 720
	ImUiColor					windowClearColor;	// Default: #1144AAFF
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

typedef enum ImAppDropType
{
	ImAppDropType_Text,
	ImAppDropType_File
} ImAppDropType;

typedef struct ImAppDropData
{
	ImAppDropType	type;
	const char*		pathOrText;
} ImAppDropData;

// TODO:
// Create a Window at given coordinates. uiFunc callback will be called every frame to build UI.
//ImAppWindow*				ImAppWindowCreate( ImAppContext* imapp, ImUiStringView title, uint32_t x, uint32_t y, uint32_t width, uint32_t height, ImAppWindowDoUiFunc uiFunc );
//void						ImAppWindowDestroy( ImAppWindow* window );

bool						ImAppWindowPopDropData( ImAppWindow* window, ImAppDropData* outData );	// data freed after tick

void						ImAppQuit( ImAppContext* imapp );

//////////////////////////////////////////////////////////////////////////
// Resources

// Resource Package
#define IMAPP_RES_PAK_INVALID_INDEX	0xffffu

typedef enum ImAppResPakType
{
	ImAppResPakType_Texture,
	ImAppResPakType_Image,
	ImAppResPakType_Skin,
	ImAppResPakType_Font,
	ImAppResPakType_Theme,
	ImAppResPakType_Blob,

	ImAppResPakType_MAX
} ImAppResPakType;

typedef enum ImAppResState
{
	ImAppResState_Loading,
	ImAppResState_Ready,
	ImAppResState_Error
} ImAppResState;

typedef struct ImAppBlob
{
	const void*			data;
	size_t				size;
} ImAppBlob;

ImAppResPak*				ImAppResourceGetDefaultPak( ImAppContext* imapp );
ImAppResPak*				ImAppResourceOpenPak( ImAppContext* imapp, const char* resourcePath );
void						ImAppResourceClosePak( ImAppContext* imapp, ImAppResPak* pak );

ImAppResState				ImAppResPakGetState( const ImAppResPak* pak );												// returns true when res pak meta data are loaded
bool						ImAppResPakPreloadResourceIndex( ImAppResPak* pak, uint16_t resIndex );						// returns true when the resource is loaded
bool						ImAppResPakPreloadResourceName( ImAppResPak* pak, ImAppResPakType type, const char* name );	// returns true when the resource is loaded
uint16_t					ImAppResPakFindResourceIndex( const ImAppResPak* pak, ImAppResPakType type, const char* name );

const ImUiImage*			ImAppResPakGetImage( ImAppResPak* pak, const char* name );
const ImUiImage*			ImAppResPakGetImageIndex( ImAppResPak* pak, uint16_t resIndex );
const ImUiSkin*				ImAppResPakGetSkin( ImAppResPak* pak, const char* name );
const ImUiSkin*				ImAppResPakGetSkinIndex( ImAppResPak* pak, uint16_t resIndex );
ImUiFont*					ImAppResPakGetFont( ImAppResPak* pak, const char* name );
ImUiFont*					ImAppResPakGetFontIndex( ImAppResPak* pak, uint16_t resIndex );
const ImUiToolboxConfig*	ImAppResPakGetTheme( ImAppResPak* pak, const char* name );
const ImUiToolboxConfig*	ImAppResPakGetThemeIndex( ImAppResPak* pak, uint16_t resIndex );
ImAppBlob					ImAppResPakGetBlob( ImAppResPak* pak, const char* name );
ImAppBlob					ImAppResPakGetBlobIndex( ImAppResPak* pak, uint16_t resIndex );

void						ImAppResPakActivateTheme( ImAppResPak* pak, const char* name );

// Image
ImAppImage*					ImAppImageLoadResource( ImAppContext* imapp, const char* resourcePath );
ImAppImage*					ImAppImageCreateRaw( ImAppContext* imapp, const void* imageData, size_t imageDataSize, int width, int height );
ImAppImage*					ImAppImageCreatePng( ImAppContext* imapp, const void* imageData, size_t imageDataSize );
ImAppImage*					ImAppImageCreateJpeg( ImAppContext* imapp, const void* imageData, size_t imageDataSize );
ImAppResState				ImAppImageGetState( ImAppContext* imapp, ImAppImage* image );
void						ImAppImageFree( ImAppContext* imapp, ImAppImage* image );

ImUiImage					ImAppImageGetImage( const ImAppImage* image );

//////////////////////////////////////////////////////////////////////////
// Types

struct ImAppContext
{
	ImUiContext*				imui;

	ImAppWindow*				defaultWindow;
	int							x;
	int							y;
	int							width;
	int							height;
};

#ifdef __cplusplus
}
#endif

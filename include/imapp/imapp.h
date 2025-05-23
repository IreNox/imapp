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
typedef struct ImAppWindow ImAppWindow;

typedef struct ImAppBlob
{
	const void*			data;
	size_t				size;
} ImAppBlob;

typedef enum ImAppDefaultWindow
{
	ImAppDefaultWindow_Resizable,
	ImAppDefaultWindow_Fullscreen,
	ImAppDefaultWindow_Disabled
} ImAppDefaultWindow;

typedef struct ImAppParameters
{
	ImUiAllocator			allocator;			// Override memory Allocator. Default: malloc/free

	int						tickIntervalMs;		// Tick interval. Use 0 to disable. Default: 0

	const char*				resPath;			// Path where resources loaded from. Use ./ for relative to executable. default: {exe_dir}/assets
	const char*				defaultResPakName;
	ImAppBlob				defaultResPakData;
	const char*				defaultThemeName;
	bool					useWindowStyle;

	const char*				defaultFontName;	// Default: arial.ttf;
	float					defaultFontSize;	// Default: 16

	const ImUiInputShortcutConfig*	shortcuts;
	size_t							shortcutCount;

	bool					shutdownAfterInit;	// Shutdown after initialization call. ImAppProgramShutdown will not be called.
	int						exitCode;			// Set exit code for shutdown after initialization

	// Only for windowed Platforms:
	ImAppDefaultWindow		windowMode;			// Opens a default Window. Default: Linux/Windows: Resizable, Android: Fullscreen
	const char*				windowTitle;		// Default: "I'm App"
	int						windowWidth;		// Default: 1280
	int						windowHeight;		// Default: 720
	ImUiColor				windowClearColor;	// Default: #1144AAFF
} ImAppParameters;

typedef void (*ImAppWindowDoUiFunc)( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow );

//////////////////////////////////////////////////////////////////////////
// Program entry points
// These function must be implemented to create a ImApp Program:

// Called at startup to initialize Program and create Context. To customize the App change values in pParameters. Must return a Program Context. Return NULL to signal an Error.
void*						ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] );

// Called for every tick to build the UI.
void						ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow );

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

bool						ImAppWindowHasFocus( const ImAppWindow* window );
void						ImAppWindowGetPosition( const ImAppWindow* window, int* outX, int* outY );
void						ImAppWindowSetPosition( ImAppWindow* window, int x, int y );
void						ImAppWindowGetViewRect( const ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight );

bool						ImAppWindowPopDropData( ImAppWindow* window, ImAppDropData* outData );	// data freed after tick

void						ImAppTrace( const char* format, ... );
void						ImAppQuit( ImAppContext* imapp, int exitCode );

//////////////////////////////////////////////////////////////////////////
// Theme

typedef struct ImAppWindowThemeTitle
{
	ImUiSkin		skin;
	ImUiColor		textColor;
	ImUiColor		backgroundColor;
} ImAppWindowThemeTitle;

typedef struct ImAppWindowThemeTitleButton
{
	ImUiSkin		skin;
	ImUiSize		size;
	ImUiColor		backgroundColor;
	ImUiColor		backgroundColorInactive;
	ImUiColor		backgroundColorHover;
	ImUiColor		backgroundColorClicked;

	ImUiImage		icon;
	ImUiSize		iconSize;
	ImUiColor		iconColor;
	ImUiColor		iconColorInactive;
	ImUiColor		iconColorHover;
	ImUiColor		iconColorClicked;
} ImAppWindowThemeTitleButton;

typedef struct ImAppWindowThemeBody
{
	ImUiSkin		skin;
	ImUiColor		color;
} ImAppWindowThemeBody;

typedef struct ImAppWindowTheme
{
	ImAppWindowThemeTitle		titleActive;
	ImAppWindowThemeTitle		titleInactive;
	ImAppWindowThemeTitleButton	titleMinimizeButton;
	ImAppWindowThemeTitleButton	titleRestoreButton;
	ImAppWindowThemeTitleButton	titleMaximizeButton;
	ImAppWindowThemeTitleButton	titleCloseButton;
	ImUiBorder					titlePadding;
	float						titleHeight;
	float						titleSpacing;

	ImAppWindowThemeBody		bodyActive;
	ImAppWindowThemeBody		bodyInactive;
	ImUiBorder					bodyPadding;
} ImAppWindowTheme;

ImUiToolboxThemeReflection	ImAppWindowThemeReflectionGet();

ImAppWindowTheme*			ImAppWindowThemeGet();
void						ImAppWindowThemeSet( const ImAppWindowTheme* windowTheme );
void						ImAppWindowThemeFillDefault( ImAppWindowTheme* windowTheme );

//////////////////////////////////////////////////////////////////////////
// Resources

// Resource Package
typedef struct ImAppResPak ImAppResPak;

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

typedef struct ImAppTheme
{
	ImUiToolboxTheme		uiTheme;
	ImAppWindowTheme		windowTheme;
} ImAppTheme;

ImAppResPak*				ImAppResourceGetDefaultPak( ImAppContext* imapp );
ImAppResPak*				ImAppResourceAddMemoryPak( ImAppContext* imapp, const void* pakData, size_t dataLength );
ImAppResPak*				ImAppResourceOpenPak( ImAppContext* imapp, const char* resourcePath );
void						ImAppResourceClosePak( ImAppContext* imapp, ImAppResPak* pak );

ImAppResState				ImAppResPakGetState( const ImAppResPak* pak );
ImAppResState				ImAppResPakLoadResourceIndex( ImAppResPak* pak, uint16_t resIndex );
ImAppResState				ImAppResPakLoadResourceName( ImAppResPak* pak, ImAppResPakType type, const char* name );
uint16_t					ImAppResPakFindResourceIndex( const ImAppResPak* pak, ImAppResPakType type, const char* name );

const ImUiImage*			ImAppResPakGetImage( ImAppResPak* pak, const char* name );
const ImUiImage*			ImAppResPakGetImageIndex( ImAppResPak* pak, uint16_t resIndex );
const ImUiSkin*				ImAppResPakGetSkin( ImAppResPak* pak, const char* name );
const ImUiSkin*				ImAppResPakGetSkinIndex( ImAppResPak* pak, uint16_t resIndex );
ImUiFont*					ImAppResPakGetFont( ImAppResPak* pak, const char* name );
ImUiFont*					ImAppResPakGetFontIndex( ImAppResPak* pak, uint16_t resIndex );
const ImAppTheme*			ImAppResPakGetTheme( ImAppResPak* pak, const char* name );
const ImAppTheme*			ImAppResPakGetThemeIndex( ImAppResPak* pak, uint16_t resIndex );
ImAppBlob					ImAppResPakGetBlob( ImAppResPak* pak, const char* name );
ImAppBlob					ImAppResPakGetBlobIndex( ImAppResPak* pak, uint16_t resIndex );

void						ImAppResPakActivateTheme( ImAppContext* imapp, ImAppResPak* pak, const char* name );

// Image
typedef struct ImAppImage ImAppImage;

ImAppImage*					ImAppImageLoadResource( ImAppContext* imapp, const char* resourcePath );
ImAppImage*					ImAppImageCreateRaw( ImAppContext* imapp, const void* imageData, size_t imageDataSize, int width, int height );
ImAppImage*					ImAppImageCreatePng( ImAppContext* imapp, const void* imageData, size_t imageDataSize );
ImAppImage*					ImAppImageCreateJpeg( ImAppContext* imapp, const void* imageData, size_t imageDataSize );
ImAppResState				ImAppImageGetState( ImAppContext* imapp, ImAppImage* image );
void						ImAppImageFree( ImAppContext* imapp, ImAppImage* image );

ImUiImage					ImAppImageGetImage( const ImAppImage* image );

#ifdef __cplusplus
}
#endif

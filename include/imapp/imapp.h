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

typedef struct ImappContext ImappContext;
typedef struct ImappWindow ImappWindow;

#define IMAPP_WINDOW_DEFAULT_POSITION ((int)0x8fffffff)

typedef struct ImappBlob
{
	const void*			data;
	size_t				size;
} ImappBlob;

typedef enum ImappWindowStyle
{
	ImappWindowStyle_Resizable,
	ImappWindowStyle_Borderless,
	ImappWindowStyle_Custom			// Use the ResPak window style
} ImappWindowStyle;

typedef enum ImappWindowState
{
	ImappWindowState_Default,
	ImappWindowState_Maximized,
	ImappWindowState_Minimized
} ImappWindowState;

typedef struct ImappWindowParameters
{
	const char*				title;
	int						x;
	int						y;
	int						width;
	int						height;
	ImappWindowStyle		style;
	ImappWindowState		state;
	ImuiColor				clearColor;
} ImappWindowParameters;

typedef struct ImappParameters
{
	ImuiAllocator			allocator;				// Override memory Allocator. Default: malloc/free

	int						tickIntervalMs;			// Tick interval. Use 0 to disable. Default: 0

	const char*				resPath;				// Path where resources loaded from. Use ./ for relative to executable. default: {exe_dir}/assets
	const char*				defaultResPakName;
	ImappBlob				defaultResPakData;
	const char*				defaultThemeName;

	const char*				defaultFontName;		// Default: arial.ttf;
	float					defaultFontSize;		// Default: 16

	const ImuiInputShortcutConfig*	shortcuts;
	size_t							shortcutCount;

	bool					shutdownAfterInit;		// Shutdown after initialization call. imappProgramShutdown will not be called.
	int						exitCode;				// Set exit code for shutdown after initialization

	// Only for windowed Platforms:
	//imappDefaultWindow		windowMode;				// Opens a default Window. Default: Linux/Windows: Resizable, Android: Fullscreen
	bool					useDefaultWindow;		// Default: true
	ImappWindowParameters	defaultWindow;			// Default: title: "I'm App", width: 1280, height: 720, style: Linux/Windows: Resizable, Android: Fullscreen, state: clear color: #1144AAFF
} ImappParameters;

typedef void (*ImappWindowDoUiFunc)( ImappContext* imapp, void* programContext, ImappWindow* appWindow, ImuiWindow* uiWindow, void* uiContext );

//////////////////////////////////////////////////////////////////////////
// Program entry points
// These function must be implemented to create a imapp Program:

// Called at startup to initialize Program and create Context. To customize the App change values in pParameters. Must return a Program Context. Return NULL to signal an Error.
void*						imappProgramInitialize( ImappParameters* parameters, int argc, char* argv[] );

// Called for every tick to build the UI.
void						imappProgramDoDefaultWindowUi( ImappContext* imapp, void* programContext, ImappWindow* appWindow, ImuiWindow* uiWindow );

// Called before shutdown. Free the Program Context here.
void						imappProgramShutdown( ImappContext* imapp, void* programContext );

//////////////////////////////////////////////////////////////////////////
// Control

typedef enum ImappDropType
{
	ImappDropType_Text,
	ImappDropType_File
} ImappDropType;

typedef struct ImappDropData
{
	ImappDropType	type;
	const char*		pathOrText;
} ImappDropData;

ImuiContext*				imappGetUi( ImappContext* imapp );

void						imappTrace( const char* format, ... );
void						imappQuit( ImappContext* imapp, int exitCode );

// Create a Window at given coordinates. uiFunc callback will be called every frame to build UI.
ImappWindow*				imappWindowCreate( ImappContext* imapp, const ImappWindowParameters* parameters, ImappWindowDoUiFunc uiFunc, void* uiContext );
void						imappWindowDestroy( ImappContext* imapp, ImappWindow* window );

bool						imappWindowIsOpen( const ImappContext* imapp, const ImappWindow* window );

bool						imappWindowHasFocus( const ImappWindow* window );
void						imappWindowGetPosition( const ImappWindow* window, int* outX, int* outY );
void						imappWindowSetPosition( ImappWindow* window, int x, int y );
void						imappWindowGetViewRect( const ImappWindow* window, int* outX, int* outY, int* outWidth, int* outHeight );

ImappWindowStyle			imappWindowGetStyle( const ImappWindow* window );
ImappWindowState			imappWindowGetState( const ImappWindow* window );
void						imappWindowSetState( ImappWindow* window, ImappWindowState state );

bool						imappWindowPopDropData( ImappWindow* window, ImappDropData* outData );	// data freed after tick

//////////////////////////////////////////////////////////////////////////
// Theme

typedef struct ImappWindowThemeTitle
{
	ImuiSkin		skin;
	ImuiColor		textColor;
	ImuiColor		backgroundColor;
} ImappWindowThemeTitle;

typedef struct ImappWindowThemeTitleButton
{
	ImuiSkin		skin;
	ImuiSize		size;
	ImuiColor		backgroundColor;
	ImuiColor		backgroundColorInactive;
	ImuiColor		backgroundColorHover;
	ImuiColor		backgroundColorClicked;

	ImuiImage		icon;
	ImuiSize		iconSize;
	ImuiColor		iconColor;
	ImuiColor		iconColorInactive;
	ImuiColor		iconColorHover;
	ImuiColor		iconColorClicked;
} ImappWindowThemeTitleButton;

typedef struct ImappWindowThemeBody
{
	ImuiSkin		skin;
	ImuiColor		color;
} ImappWindowThemeBody;

typedef struct ImappWindowTheme
{
	ImappWindowThemeTitle		titleActive;
	ImappWindowThemeTitle		titleInactive;
	ImappWindowThemeTitleButton	titleMinimizeButton;
	ImappWindowThemeTitleButton	titleRestoreButton;
	ImappWindowThemeTitleButton	titleMaximizeButton;
	ImappWindowThemeTitleButton	titleCloseButton;
	ImuiBorder					titlePadding;
	float						titleHeight;
	float						titleSpacing;

	ImappWindowThemeBody		bodyActive;
	ImappWindowThemeBody		bodyInactive;
	ImuiBorder					bodyPadding;
} ImappWindowTheme;

ImuiToolboxThemeReflection	imappWindowThemeReflectionGet();

ImappWindowTheme*			imappWindowThemeGet();
void						imappWindowThemeSet( const ImappWindowTheme* windowTheme );
void						imappWindowThemeFillDefault( ImappWindowTheme* windowTheme );

ImuiFont*					imappGetDefaultFont( const ImappContext* imapp );

//////////////////////////////////////////////////////////////////////////
// Resources

// Resource Package
typedef struct ImappResPak ImappResPak;

#define IMAPP_RES_PAK_INVALID_INDEX	0xffffu

typedef enum ImappResPakType
{
	ImappResPakType_Texture,
	ImappResPakType_Image,
	ImappResPakType_Skin,
	ImappResPakType_Font,
	ImappResPakType_Theme,
	ImappResPakType_Blob,

	ImappResPakType_MAX
} ImappResPakType;

typedef enum ImappResState
{
	ImappResState_Loading,
	ImappResState_Ready,
	ImappResState_Error
} ImappResState;

typedef struct ImappTheme
{
	ImuiToolboxTheme		uiTheme;
	ImappWindowTheme		windowTheme;
} ImappTheme;

ImappResPak*				imappResourceGetDefaultPak( ImappContext* imapp );
ImappResPak*				imappResourceAddMemoryPak( ImappContext* imapp, const void* pakData, size_t dataLength );
ImappResPak*				imappResourceOpenPak( ImappContext* imapp, const char* resourcePath );
void						imappResourceClosePak( ImappContext* imapp, ImappResPak* pak );

ImappResState				imappResPakGetState( const ImappResPak* pak );
ImappResState				imappResPakLoadResourceIndex( ImappResPak* pak, uint16_t resIndex );
ImappResState				imappResPakLoadResourceName( ImappResPak* pak, ImappResPakType type, const char* name );
uint16_t					imappResPakFindResourceIndex( const ImappResPak* pak, ImappResPakType type, const char* name );

const ImuiImage*			imappResPakGetImage( ImappResPak* pak, const char* name );
const ImuiImage*			imappResPakGetImageIndex( ImappResPak* pak, uint16_t resIndex );
const ImuiSkin*				imappResPakGetSkin( ImappResPak* pak, const char* name );
const ImuiSkin*				imappResPakGetSkinIndex( ImappResPak* pak, uint16_t resIndex );
ImuiFont*					imappResPakGetFont( ImappResPak* pak, const char* name );
ImuiFont*					imappResPakGetFontIndex( ImappResPak* pak, uint16_t resIndex );
const ImappTheme*			imappResPakGetTheme( ImappResPak* pak, const char* name );
const ImappTheme*			imappResPakGetThemeIndex( ImappResPak* pak, uint16_t resIndex );
ImappBlob					imappResPakGetBlob( ImappResPak* pak, const char* name );
ImappBlob					imappResPakGetBlobIndex( ImappResPak* pak, uint16_t resIndex );

void						imappResPakActivateTheme( ImappContext* imapp, ImappResPak* pak, const char* name );

// Image
typedef struct ImappImage ImappImage;

ImappImage*					imappImageLoadResource( ImappContext* imapp, const char* resourcePath );
ImappImage*					imappImageCreateRaw( ImappContext* imapp, const void* imageData, size_t imageDataSize, int width, int height );
ImappImage*					imappImageCreatePng( ImappContext* imapp, const void* imageData, size_t imageDataSize );
ImappImage*					imappImageCreateJpeg( ImappContext* imapp, const void* imageData, size_t imageDataSize );
ImappResState				imappImageGetState( ImappContext* imapp, ImappImage* image );
void						imappImageFree( ImappContext* imapp, ImappImage* image );

ImuiImage					imappImageGetImage( const ImappImage* image );

#ifdef __cplusplus
}
#endif

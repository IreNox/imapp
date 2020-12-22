#pragma once

//////////////////////////////////////////////////////////////////////////
// Core

struct ImAppPlatform;
typedef struct ImAppPlatform ImAppPlatform;

void					ImAppShowError( ImAppPlatform* pPlatform, const char* pMessage );

//////////////////////////////////////////////////////////////////////////
// Shared Libraries

struct ImAppSharedLib;
typedef struct ImAppSharedLib* ImAppSharedLibHandle;

ImAppSharedLibHandle	ImAppSharedLibOpen( const char* pSharedLibName );
void					ImAppSharedLibClose( ImAppSharedLibHandle libHandle );
void*					ImAppSharedLibGetFunction( ImAppSharedLibHandle libHandle, const char* pFunctionName );

//////////////////////////////////////////////////////////////////////////
// Event

//typedef enum
//{
//	// Window
//	ImAppEventType_WindowClose,
//
//	// Input
//	ImAppEventType_KeyDown,
//	ImAppEventType_KeyUp,
//	ImAppEventType_ButtonDown,
//	ImAppEventType_ButtonUp,
//	ImAppEventType_Motion
//} ImAppEventType;
//
//typedef struct
//{
//	ImAppEventType			type;
//} ImAppWindowEvent;
//
//typedef enum
//{
//	ImAppInputKey_Bla
//} ImAppInputKey;
//
//typedef struct
//{
//	ImAppEventType			type;
//	ImAppInputKey			key;
//} ImAppInputKeyEvent;
//
//typedef enum
//{
//	ImAppInputButton_Left,
//	ImAppInputButton_Right,
//	ImAppInputButton_Middle
//} ImAppInputButton;
//
//typedef struct
//{
//	ImAppEventType			type;
//	ImAppInputButton		button;
//} ImAppInputButtonEvent;
//
//typedef union
//{
//	ImAppEventType			type;
//
//	ImAppWindowEvent		window;
//	ImAppInputKeyEvent		key;
//	ImAppInputButtonEvent	button;
//} ImAppEvent;

//////////////////////////////////////////////////////////////////////////
// Window

struct ImAppWindow;
typedef struct ImAppWindow ImAppWindow;

typedef enum ImAppWindowState
{
	ImAppWindowState_Default,
	ImAppWindowState_Maximized,
	ImAppWindowState_Minimized
} ImAppWindowState;

ImAppWindow*			ImAppWindowCreate( ImAppPlatform* pPlatform, const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state );
void					ImAppWindowDestroy( ImAppWindow* pWindow );

int64_t					ImAppWindowTick( ImAppWindow* pWindow, int64_t lastTickValue, int64_t tickInterval, struct nk_context* pNkContext );
bool					ImAppWindowPresent( ImAppWindow* pWindow );

bool					ImAppWindowIsOpen( ImAppWindow* pWindow );
void					ImAppWindowGetViewRect( int* pX, int* pY, int* pWidth, int* pHeight, ImAppWindow* pWindow );
void					ImAppWindowGetSize( int* pWidth, int* pHeight, ImAppWindow* pWindow );
void					ImAppWindowGetPosition( int* pX, int* pY, ImAppWindow* pWindow );
ImAppWindowState		ImAppWindowGetState( ImAppWindow* pWindow );

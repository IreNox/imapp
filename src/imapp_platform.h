#pragma once

#include "imapp_internal.h"
#include "imapp_main.h"

//////////////////////////////////////////////////////////////////////////
// Core

typedef struct ImAppPlatform ImAppPlatform;

bool					ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath );
void					ImAppPlatformShutdown( ImAppPlatform* platform );

void					ImAppPlatformShowError( ImAppPlatform* platform, const char* pMessage );

void					ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor );

//////////////////////////////////////////////////////////////////////////
// Window

typedef struct ImAppEventQueue ImAppEventQueue;
typedef struct ImAppWindow ImAppWindow;

typedef enum ImAppWindowState ImAppWindowState;
enum ImAppWindowState
{
	ImAppWindowState_Default,
	ImAppWindowState_Maximized,
	ImAppWindowState_Minimized
};

ImAppWindow*			ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state );
void					ImAppPlatformWindowDestroy( ImAppWindow* window );

bool					ImAppPlatformWindowCreateGlContext( ImAppWindow* window );
void					ImAppPlatformWindowDestroyGlContext( ImAppWindow* window );

int64_t					ImAppPlatformWindowTick( ImAppWindow* window, int64_t lastTickValue, int64_t tickInterval );
bool					ImAppPlatformWindowPresent( ImAppWindow* window );

ImAppEventQueue*		ImAppPlatformWindowGetEventQueue( ImAppWindow* window );

void					ImAppPlatformWindowGetViewRect( int* pX, int* pY, int* pWidth, int* pHeight, ImAppWindow* window );
void					ImAppPlatformWindowGetSize( int* pWidth, int* pHeight, ImAppWindow* window );
void					ImAppPlatformWindowGetPosition( int* pX, int* pY, ImAppWindow* window );
ImAppWindowState		ImAppPlatformWindowGetState( ImAppWindow* window );

//////////////////////////////////////////////////////////////////////////
// Resources

typedef struct ImAppBlob ImAppBlob;
struct ImAppBlob
{
	const void*			data;
	size_t				size;
};

ImAppBlob				ImAppPlatformResourceLoad( ImAppPlatform* platform, ImUiStringView resourceName );
ImAppBlob				ImAppPlatformResourceLoadSystemFont( ImAppPlatform* platform, ImUiStringView resourceName );

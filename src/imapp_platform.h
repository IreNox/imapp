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
// Window

typedef struct ImAppWindow ImAppWindow;

enum ImAppWindowState
{
	ImAppWindowState_Default,
	ImAppWindowState_Maximized,
	ImAppWindowState_Minimized
};
typedef enum ImAppWindowState ImAppWindowState;

ImAppWindow*			ImAppWindowCreate( ImAppAllocator* pAllocator, ImAppPlatform* pPlatform, const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state );
void					ImAppWindowDestroy( ImAppWindow* pWindow );

int64_t					ImAppWindowTick( ImAppWindow* pWindow, int64_t lastTickValue, int64_t tickInterval );
bool					ImAppWindowPresent( ImAppWindow* pWindow );

ImAppEventQueue*		ImAppWindowGetEventQueue( ImAppWindow* pWindow );

void					ImAppWindowGetViewRect( int* pX, int* pY, int* pWidth, int* pHeight, ImAppWindow* pWindow );
void					ImAppWindowGetSize( int* pWidth, int* pHeight, ImAppWindow* pWindow );
void					ImAppWindowGetPosition( int* pX, int* pY, ImAppWindow* pWindow );
ImAppWindowState		ImAppWindowGetState( ImAppWindow* pWindow );

//////////////////////////////////////////////////////////////////////////
// Resources

struct ImAppResource
{
	const void*			pData;
	size_t				size;
};
typedef struct ImAppResource ImAppResource;

ImAppResource			ImAppResourceLoad( ImAppPlatform* pPlatform, ImAppAllocator* pAllocator, const char* pResourceName );

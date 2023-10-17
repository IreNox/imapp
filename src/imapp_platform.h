#pragma once

#include "imapp_types.h"
#include "imapp_main.h"

//////////////////////////////////////////////////////////////////////////
// Core

typedef struct ImAppPlatform ImAppPlatform;

bool					ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath );
void					ImAppPlatformShutdown( ImAppPlatform* platform );

sint64					ImAppPlatformTick( ImAppPlatform* platform, sint64 lastTickValue, sint64 tickInterval );

void					ImAppPlatformShowError( ImAppPlatform* platform, const char* message );

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

ImAppWindow*			ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowState state );
void					ImAppPlatformWindowDestroy( ImAppWindow* window );

bool					ImAppPlatformWindowCreateGlContext( ImAppWindow* window );
void					ImAppPlatformWindowDestroyGlContext( ImAppWindow* window );

void					ImAppPlatformWindowUpdate( ImAppWindow* window );
bool					ImAppPlatformWindowPresent( ImAppWindow* window );

ImAppEventQueue*		ImAppPlatformWindowGetEventQueue( ImAppWindow* window );

void					ImAppPlatformWindowGetViewRect( ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight );
void					ImAppPlatformWindowGetSize( ImAppWindow* window, int* outWidth, int* outHeight );
void					ImAppPlatformWindowGetPosition( ImAppWindow* window, int* outX, int* outY );
ImAppWindowState		ImAppPlatformWindowGetState( ImAppWindow* window );

//////////////////////////////////////////////////////////////////////////
// Resources

ImAppBlob				ImAppPlatformResourceLoad( ImAppPlatform* platform, const char* resourceName );
ImAppBlob				ImAppPlatformResourceLoadRange( ImAppPlatform* platform, const char* resourceName, uintsize offset, uintsize length );
ImAppBlob				ImAppPlatformResourceLoadSystemFont( ImAppPlatform* platform, const char* fontName );

//////////////////////////////////////////////////////////////////////////
// Threading

typedef struct ImAppThread ImAppThread;
typedef struct ImAppMutex ImAppMutex;
typedef struct ImAppSemaphore ImAppSemaphore;

typedef void (*ImAppThreadFunc)( void* arg );

typedef struct
{
	uint32 value;
} ImAppAtomic32;

ImAppThread*			ImAppPlatformThreadCreate( ImAppPlatform* platform, const char* name, ImAppThreadFunc func, void* arg );
void					ImAppPlatformThreadDestroy( ImAppThread* thread );
bool					ImAppPlatformThreadIsRunning( const ImAppThread* thread );

ImAppMutex*				ImAppPlatformMutexCreate( ImAppPlatform* platform );
void					ImAppPlatformMutexDestroy( ImAppPlatform* platform, ImAppMutex* mutex );
void					ImAppPlatformMutexLock( ImAppMutex* mutex );
void					ImAppPlatformMutexUnlock( ImAppMutex* mutex );

ImAppSemaphore*			ImAppPlatformSemaphoreCreate( ImAppPlatform* platform );
void					ImAppPlatformSemaphoreDestroy( ImAppPlatform* platform, ImAppSemaphore* semaphore );
void					ImAppPlatformSemaphoreInc( ImAppSemaphore* semaphore );
bool					ImAppPlatformSemaphoreDec( ImAppSemaphore* semaphore, bool wait );

uint32					ImAppPlatformAtomicGet( const ImAppAtomic32* atomic );
uint32					ImAppPlatformAtomicSet( ImAppAtomic32* atomic, uint32 value );
uint32					ImAppPlatformAtomicInc( ImAppAtomic32* atomic );
uint32					ImAppPlatformAtomicDec( ImAppAtomic32* atomic );

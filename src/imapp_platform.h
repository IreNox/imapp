#pragma once

#include "imapp/imapp.h"

#include "imapp_types.h"
#include "imapp_main.h"

//////////////////////////////////////////////////////////////////////////
// Core

typedef struct ImAppPlatform ImAppPlatform;

bool					imappPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath );
void					imappPlatformShutdown( ImAppPlatform* platform );

sint64					imappPlatformTick( ImAppPlatform* platform, sint64 lastTickValue, sint64 tickIntervalMs );
double					imappPlatformTicksToSeconds( ImAppPlatform* platform, sint64 tickValue );

void					imappPlatformShowError( ImAppPlatform* platform, const char* message );

void					imappPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor );
void					imappPlatformSetClipboardText( ImAppPlatform* platform, const char* text );
void					imappPlatformGetClipboardText( ImAppPlatform* platform, ImUiContext* imui );

//////////////////////////////////////////////////////////////////////////
// Window

typedef struct ImAppEventQueue ImAppEventQueue;
typedef struct ImAppWindow ImAppWindow;

typedef enum ImAppWindowDeviceState
{
	ImAppWindowDeviceState_Ok,
	ImAppWindowDeviceState_NewDevice,
	ImAppWindowDeviceState_NoDevice,
	ImAppWindowDeviceState_DeviceLost
} ImAppWindowDeviceState;

typedef void(*ImAppPlatformWindowUpdateCallback)( ImAppWindow* window, void* arg );

ImAppWindow*			imappPlatformWindowCreate( ImAppPlatform* platform, const ImAppWindowParameters* parameters );
void					imappPlatformWindowDestroy( ImAppWindow* window );

ImAppWindowDeviceState	imappPlatformWindowGetGlContextState( const ImAppWindow* window );

void					imappPlatformWindowUpdate( ImAppWindow* window, ImAppPlatformWindowUpdateCallback callback, void* arg );
bool					imappPlatformWindowBeginRender( ImAppWindow* window );
bool					imappPlatformWindowEndRender( ImAppWindow* window );

ImAppEventQueue*		imappPlatformWindowGetEventQueue( ImAppWindow* window );

bool					imappPlatformWindowPopDropData( ImAppWindow* window, ImAppDropData* outData );

void					imappPlatformWindowGetViewRect( const ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight );
bool					imappPlatformWindowHasFocus( const ImAppWindow* window );
void					imappPlatformWindowGetSize( const ImAppWindow* window, int* outWidth, int* outHeight );
void					imappPlatformWindowSetSize( ImAppWindow* window, int width, int height );
void					imappPlatformWindowGetPosition( const ImAppWindow* window, int* outX, int* outY );
void					imappPlatformWindowSetPosition( const ImAppWindow* window, int x, int y );
ImAppWindowStyle		imappPlatformWindowGetStyle( const ImAppWindow* window );
ImAppWindowState		imappPlatformWindowGetState( const ImAppWindow* window );
void					imappPlatformWindowSetState( ImAppWindow* window, ImAppWindowState state );
const char*				imappPlatformWindowGetTitle( const ImAppWindow* window );
void					imappPlatformWindowSetTitle( ImAppWindow* window, const char* title );
void					imappPlatformWindowSetTitleBounds( ImAppWindow* window, int height, int buttonsX );
float					imappPlatformWindowGetDpiScale( const ImAppWindow* window );
void					imappPlatformWindowClose( ImAppWindow* window );

//////////////////////////////////////////////////////////////////////////
// Files/Resources

typedef struct ImAppFile ImAppFile;

typedef struct ImAppFileWatcher ImAppFileWatcher;

typedef struct ImAppFileWatchEvent
{
	const char*			path;
} ImAppFileWatchEvent;

void					imappPlatformResourceGetPath( ImAppPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName );
ImAppBlob				imappPlatformResourceLoad( ImAppPlatform* platform, const char* resourceName );
ImAppBlob				imappPlatformResourceLoadRange( ImAppPlatform* platform, const char* resourceName, uintsize offset, uintsize length );
ImAppFile*				imappPlatformResourceOpen( ImAppPlatform* platform, const char* resourceName );
uintsize				imappPlatformResourceRead( ImAppFile* file, void* outData, uintsize length, uintsize offset );
void					imappPlatformResourceClose( ImAppPlatform* platform, ImAppFile* file );
ImAppBlob				imappPlatformResourceLoadSystemFont( ImAppPlatform* platform, const char* fontName );
void					imappPlatformResourceFree( ImAppPlatform* platform, ImAppBlob blob );

ImAppFileWatcher*		imappPlatformFileWatcherCreate( ImAppPlatform* platform );
void					imappPlatformFileWatcherDestroy( ImAppPlatform* platform, ImAppFileWatcher* watcher );
void					imappPlatformFileWatcherAddPath( ImAppFileWatcher* watcher, const char* path );
void					imappPlatformFileWatcherRemovePath( ImAppFileWatcher* watcher, const char* path );
bool					imappPlatformFileWatcherPopEvent( ImAppFileWatcher* watcher, ImAppFileWatchEvent* outEvent );

//////////////////////////////////////////////////////////////////////////
// Threading

typedef struct ImAppThread ImAppThread;
typedef struct ImAppMutex ImAppMutex;
typedef struct ImAppSemaphore ImAppSemaphore;

typedef void (*ImAppThreadFunc)( void* arg );

ImAppThread*			imappPlatformThreadCreate( ImAppPlatform* platform, const char* name, ImAppThreadFunc func, void* arg );
void					imappPlatformThreadDestroy( ImAppThread* thread );
bool					imappPlatformThreadIsRunning( const ImAppThread* thread );

ImAppMutex*				imappPlatformMutexCreate( ImAppPlatform* platform );
void					imappPlatformMutexDestroy( ImAppPlatform* platform, ImAppMutex* mutex );
void					imappPlatformMutexLock( ImAppMutex* mutex );
void					imappPlatformMutexUnlock( ImAppMutex* mutex );

ImAppSemaphore*			imappPlatformSemaphoreCreate( ImAppPlatform* platform );
void					imappPlatformSemaphoreDestroy( ImAppPlatform* platform, ImAppSemaphore* semaphore );
void					imappPlatformSemaphoreInc( ImAppSemaphore* semaphore );
bool					imappPlatformSemaphoreDec( ImAppSemaphore* semaphore, bool wait );


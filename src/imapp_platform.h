#pragma once

#include "imapp/imapp.h"

#include "imapp_types.h"
#include "imapp_main.h"

//////////////////////////////////////////////////////////////////////////
// Core

typedef struct ImAppPlatform ImAppPlatform;

bool					ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath );
void					ImAppPlatformShutdown( ImAppPlatform* platform );

sint64					ImAppPlatformTick( ImAppPlatform* platform, sint64 lastTickValue, sint64 tickIntervalMs );
double					ImAppPlatformTicksToSeconds( sint64 tickValue );

void					ImAppPlatformShowError( ImAppPlatform* platform, const char* message );

void					ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor );
void					ImAppPlatformSetClipboardText( ImAppPlatform* platform, const char* text );

void					ImAppPlatformGetClipboardText( ImAppPlatform* platform, ImUiContext* imui );

//////////////////////////////////////////////////////////////////////////
// Window

typedef struct ImAppEventQueue ImAppEventQueue;
typedef struct ImAppWindow ImAppWindow;

typedef enum ImAppWindowStyle
{
	ImAppWindowStyle_Resizable,
	ImAppWindowStyle_Borderless,
	ImAppWindowStyle_Custom
} ImAppWindowStyle;

typedef enum ImAppWindowState
{
	ImAppWindowState_Default,
	ImAppWindowState_Maximized,
	ImAppWindowState_Minimized
} ImAppWindowState;

typedef enum ImAppWindowDeviceState
{
	ImAppWindowDeviceState_Ok,
	ImAppWindowDeviceState_NewDevice,
	ImAppWindowDeviceState_NoDevice,
	ImAppWindowDeviceState_DeviceLost
} ImAppWindowDeviceState;

typedef void(*ImAppPlatformWindowUpdateCallback)( ImAppWindow* window, void* arg );

ImAppWindow*			ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowStyle style, ImAppWindowState state, ImAppWindowDoUiFunc uiFunc );
void					ImAppPlatformWindowDestroy( ImAppWindow* window );

bool					ImAppPlatformWindowCreateGlContext( ImAppWindow* window );
void					ImAppPlatformWindowDestroyGlContext( ImAppWindow* window );
ImAppWindowDeviceState	ImAppPlatformWindowGetGlContextState( const ImAppWindow* window );

void					ImAppPlatformWindowUpdate( ImAppWindow* window, ImAppPlatformWindowUpdateCallback callback, void* arg );
bool					ImAppPlatformWindowPresent( ImAppWindow* window );

ImAppEventQueue*		ImAppPlatformWindowGetEventQueue( ImAppWindow* window );
ImAppWindowDoUiFunc		ImAppPlatformWindowGetUiFunc( ImAppWindow* window );

bool					ImAppPlatformWindowPopDropData( ImAppWindow* window, ImAppDropData* outData );

void					ImAppPlatformWindowGetViewRect( const ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight );
bool					ImAppPlatformWindowHasFocus( const ImAppWindow* window );
void					ImAppPlatformWindowGetSize( const ImAppWindow* window, int* outWidth, int* outHeight );
void					ImAppPlatformWindowSetSize( ImAppWindow* window, int width, int height );
void					ImAppPlatformWindowGetPosition( const ImAppWindow* window, int* outX, int* outY );
void					ImAppPlatformWindowSetPosition( const ImAppWindow* window, int x, int y );
ImAppWindowState		ImAppPlatformWindowGetState( const ImAppWindow* window );
void					ImAppPlatformWindowSetState( ImAppWindow* window, ImAppWindowState state );
const char*				ImAppPlatformWindowGetTitle( const ImAppWindow* window );
void					ImAppPlatformWindowSetTitle( ImAppWindow* window, const char* title );
void					ImAppPlatformWindowSetTitleBounds( ImAppWindow* window, int height, int buttonsX );
float					ImAppPlatformWindowGetDpiScale( const ImAppWindow* window );
void					ImAppPlatformWindowClose( ImAppWindow* window );

//////////////////////////////////////////////////////////////////////////
// Files/Resources

typedef struct ImAppFile ImAppFile;

typedef struct ImAppFileWatcher ImAppFileWatcher;

typedef struct ImAppFileWatchEvent
{
	const char*			path;
} ImAppFileWatchEvent;

void					ImAppPlatformResourceGetPath( ImAppPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName );
ImAppBlob				ImAppPlatformResourceLoad( ImAppPlatform* platform, const char* resourceName );
ImAppBlob				ImAppPlatformResourceLoadRange( ImAppPlatform* platform, const char* resourceName, uintsize offset, uintsize length );
ImAppFile*				ImAppPlatformResourceOpen( ImAppPlatform* platform, const char* resourceName );
uintsize				ImAppPlatformResourceRead( ImAppFile* file, void* outData, uintsize length, uintsize offset );
void					ImAppPlatformResourceClose( ImAppPlatform* platform, ImAppFile* file );
ImAppBlob				ImAppPlatformResourceLoadSystemFont( ImAppPlatform* platform, const char* fontName );
void					ImAppPlatformResourceFree( ImAppPlatform* platform, ImAppBlob blob );

ImAppFileWatcher*		ImAppPlatformFileWatcherCreate( ImAppPlatform* platform );
void					ImAppPlatformFileWatcherDestroy( ImAppPlatform* platform, ImAppFileWatcher* watcher );
void					ImAppPlatformFileWatcherAddPath( ImAppFileWatcher* watcher, const char* path );
void					ImAppPlatformFileWatcherRemovePath( ImAppFileWatcher* watcher, const char* path );
bool					ImAppPlatformFileWatcherPopEvent( ImAppFileWatcher* watcher, ImAppFileWatchEvent* outEvent );

//////////////////////////////////////////////////////////////////////////
// Threading

typedef struct ImAppThread ImAppThread;
typedef struct ImAppMutex ImAppMutex;
typedef struct ImAppSemaphore ImAppSemaphore;

typedef void (*ImAppThreadFunc)( void* arg );

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


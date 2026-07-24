#pragma once

#include "imapp/imapp.h"

#include "imapp_types.h"
#include "imapp_main.h"

//////////////////////////////////////////////////////////////////////////
// Core

typedef struct ImappPlatform ImappPlatform;

bool					imappPlatformInitialize( ImappPlatform* platform, ImuiAllocator* allocator, const char* resourcePath );
void					imappPlatformShutdown( ImappPlatform* platform );

sint64					imappPlatformTick( ImappPlatform* platform, sint64 lastTickValue, sint64 tickIntervalMs );
double					imappPlatformTicksToSeconds( ImappPlatform* platform, sint64 tickValue );

void					imappPlatformShowError( ImappPlatform* platform, const char* message );

void					imappPlatformSetMouseCursor( ImappPlatform* platform, ImuiInputMouseCursor cursor );
void					imappPlatformSetClipboardText( ImappPlatform* platform, const char* text );
void					imappPlatformGetClipboardText( ImappPlatform* platform, ImuiContext* imui );

//////////////////////////////////////////////////////////////////////////
// Window

typedef struct ImappEventQueue ImappEventQueue;
typedef struct ImappWindow ImappWindow;

typedef enum ImappWindowDeviceState
{
	ImappWindowDeviceState_Ok,
	ImappWindowDeviceState_NewDevice,
	ImappWindowDeviceState_NoDevice,
	ImappWindowDeviceState_DeviceLost
} ImappWindowDeviceState;

typedef void(*imappPlatformWindowUpdateCallback)( ImappWindow* window, void* arg );

ImappWindow*			imappPlatformWindowCreate( ImappPlatform* platform, const ImappWindowParameters* parameters );
void					imappPlatformWindowDestroy( ImappWindow* window );

ImappWindowDeviceState	imappPlatformWindowGetGlContextState( const ImappWindow* window );

void					imappPlatformWindowUpdate( ImappWindow* window, imappPlatformWindowUpdateCallback callback, void* arg );
bool					imappPlatformWindowBeginRender( ImappWindow* window );
bool					imappPlatformWindowEndRender( ImappWindow* window );

ImappEventQueue*		imappPlatformWindowGetEventQueue( ImappWindow* window );

bool					imappPlatformWindowPopDropData( ImappWindow* window, ImappDropData* outData );

void					imappPlatformWindowGetViewRect( const ImappWindow* window, int* outX, int* outY, int* outWidth, int* outHeight );
bool					imappPlatformWindowHasFocus( const ImappWindow* window );
void					imappPlatformWindowGetSize( const ImappWindow* window, int* outWidth, int* outHeight );
void					imappPlatformWindowSetSize( ImappWindow* window, int width, int height );
void					imappPlatformWindowGetPosition( const ImappWindow* window, int* outX, int* outY );
void					imappPlatformWindowSetPosition( const ImappWindow* window, int x, int y );
ImappWindowStyle		imappPlatformWindowGetStyle( const ImappWindow* window );
ImappWindowState		imappPlatformWindowGetState( const ImappWindow* window );
void					imappPlatformWindowSetState( ImappWindow* window, ImappWindowState state );
const char*				imappPlatformWindowGetTitle( const ImappWindow* window );
void					imappPlatformWindowSetTitle( ImappWindow* window, const char* title );
void					imappPlatformWindowSetTitleBounds( ImappWindow* window, int height, int buttonsX );
float					imappPlatformWindowGetDpiScale( const ImappWindow* window );
void					imappPlatformWindowClose( ImappWindow* window );

//////////////////////////////////////////////////////////////////////////
// Files/Resources

typedef struct ImappFile ImappFile;

typedef struct ImappFileWatcher ImappFileWatcher;

typedef struct ImappFileWatchEvent
{
	const char*			path;
} ImappFileWatchEvent;

void					imappPlatformResourceGetPath( ImappPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName );
ImappBlob				imappPlatformResourceLoad( ImappPlatform* platform, const char* resourceName );
ImappBlob				imappPlatformResourceLoadRange( ImappPlatform* platform, const char* resourceName, uintsize offset, uintsize length );
ImappFile*				imappPlatformResourceOpen( ImappPlatform* platform, const char* resourceName );
uintsize				imappPlatformResourceRead( ImappFile* file, void* outData, uintsize length, uintsize offset );
void					imappPlatformResourceClose( ImappPlatform* platform, ImappFile* file );
ImappBlob				imappPlatformResourceLoadSystemFont( ImappPlatform* platform, const char* fontName );
void					imappPlatformResourceFree( ImappPlatform* platform, ImappBlob blob );

ImappFileWatcher*		imappPlatformFileWatcherCreate( ImappPlatform* platform );
void					imappPlatformFileWatcherDestroy( ImappPlatform* platform, ImappFileWatcher* watcher );
void					imappPlatformFileWatcherAddPath( ImappFileWatcher* watcher, const char* path );
void					imappPlatformFileWatcherRemovePath( ImappFileWatcher* watcher, const char* path );
bool					imappPlatformFileWatcherPopEvent( ImappFileWatcher* watcher, ImappFileWatchEvent* outEvent );

//////////////////////////////////////////////////////////////////////////
// Threading

typedef struct ImappThread ImappThread;
typedef struct ImappMutex ImappMutex;
typedef struct ImappSemaphore ImappSemaphore;

typedef void (*ImappThreadFunc)( void* arg );

ImappThread*			imappPlatformThreadCreate( ImappPlatform* platform, const char* name, ImappThreadFunc func, void* arg );
void					imappPlatformThreadDestroy( ImappThread* thread );
bool					imappPlatformThreadIsRunning( const ImappThread* thread );

ImappMutex*				imappPlatformMutexCreate( ImappPlatform* platform );
void					imappPlatformMutexDestroy( ImappPlatform* platform, ImappMutex* mutex );
void					imappPlatformMutexLock( ImappMutex* mutex );
void					imappPlatformMutexUnlock( ImappMutex* mutex );

ImappSemaphore*			imappPlatformSemaphoreCreate( ImappPlatform* platform );
void					imappPlatformSemaphoreDestroy( ImappPlatform* platform, ImappSemaphore* semaphore );
void					imappPlatformSemaphoreInc( ImappSemaphore* semaphore );
bool					imappPlatformSemaphoreDec( ImappSemaphore* semaphore, bool wait );


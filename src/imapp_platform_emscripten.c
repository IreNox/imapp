#include "imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB ) && IMAPP_DISABLED( IMAPP_PLATFORM_SDL )

struct ImAppPlatform
{
	ImUiAllocator*	allocator;
};

int main( int argc, char* argv[] )
{
	ImAppPlatform platform = { 0 };

   	const int result = ImAppMain( &platform, argc, argv );

	return result;
}

bool ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
    return true;
}

void ImAppPlatformShutdown( ImAppPlatform* platform )
{

}

sint64 ImAppPlatformTick( ImAppPlatform* platform, sint64 lastTickValue, sint64 tickInterval )
{
    return 1;
}

void ImAppPlatformShowError( ImAppPlatform* platform, const char* message )
{

}

void ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor )
{

}

void ImAppPlatformSetClipboardText( ImAppPlatform* platform, const char* text )
{

}

void ImAppPlatformGetClipboardText( ImAppPlatform* platform, ImUiContext* imui )
{

}

ImAppWindow* ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowStyle style, ImAppWindowState state )
{

}

void ImAppPlatformWindowDestroy( ImAppWindow* window )
{

}

bool ImAppPlatformWindowCreateGlContext( ImAppWindow* window )
{

}

void ImAppPlatformWindowDestroyGlContext( ImAppWindow* window )
{

}

void					ImAppPlatformWindowUpdate( ImAppWindow* window, ImAppPlatformWindowUpdateCallback callback, void* arg );
bool					ImAppPlatformWindowPresent( ImAppWindow* window );

ImAppEventQueue*		ImAppPlatformWindowGetEventQueue( ImAppWindow* window );

bool					ImAppPlatformWindowPopDropData( ImAppWindow* window, ImAppDropData* outData );

void					ImAppPlatformWindowGetViewRect( const ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight );
void					ImAppPlatformWindowGetSize( const ImAppWindow* window, int* outWidth, int* outHeight );
void					ImAppPlatformWindowGetPosition( const ImAppWindow* window, int* outX, int* outY );
ImAppWindowState		ImAppPlatformWindowGetState( const ImAppWindow* window );
float					ImAppPlatformWindowGetDpiScale( const ImAppWindow* window );

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

#endif

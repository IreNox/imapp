#include "imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB ) && IMAPP_DISABLED( IMAPP_PLATFORM_SDL )

#include "imapp_debug.h"
#include "imapp_event_queue.h"
#include "imapp_internal.h"

#include <EGL/egl.h>
#include <emscripten.h>
#include <GLES2/gl2.h>
#include <pthread.h>
#include <semaphore.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

struct ImAppPlatform
{
	ImUiAllocator*		allocator;

	Display*			xDisplay;
};

struct ImAppWindow
{
	ImAppPlatform*		platform;
	ImAppEventQueue		eventQueue;

	Window				xWindow;
	EGLDisplay			eglDisplay;
	EGLContext			eglContext;
	EGLSurface			eglSurface;

	int					width;
	int					height;

	ImAppWindowDoUiFunc uiFunc;
};

struct ImAppThread
{
	ImAppPlatform*				platform;

	pthread_t					handle;

	ImAppThreadFunc				func;
	void*						arg;

	uint8						hasExit;
};

static void*	ImAppPlatformThreadEntry( void* voidThread );

int main()
{
	ImAppPlatform platform = { 0 };

	const int result = ImAppMain( &platform, 0, NULL );

	return result;
}

bool ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	platform->xDisplay = XOpenDisplay( NULL );
	if ( !platform->xDisplay )
	{
		IMAPP_DEBUG_LOGE( "Failed to open Display." );
		return false;
	}

	return true;
}

void ImAppPlatformShutdown( ImAppPlatform* platform )
{
	if( platform->xDisplay )
	{
		//XCloseDisplay( platform->xDisplay );
		platform->xDisplay = NULL;
	}

	platform->allocator = NULL;
}

sint64 ImAppPlatformTick( ImAppPlatform* platform, sint64 lastTickValue, sint64 tickInterval )
{
	return 1;
}

void ImAppPlatformShowError( ImAppPlatform* platform, const char* message )
{
	EM_ASM( {
		alert(UTF8ToString($0));
	}, message );
}

void ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor )
{
		// TODO
}

void ImAppPlatformSetClipboardText( ImAppPlatform* platform, const char* text )
{
	// TODO
}

void ImAppPlatformGetClipboardText( ImAppPlatform* platform, ImUiContext* imui )
{
	// TODO
}

ImAppWindow* ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowStyle style, ImAppWindowState state, ImAppWindowDoUiFunc uiFunc )
{
	ImAppWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppWindow );

	window->platform	= platform;
	window->width		= width;
	window->height		= height;
	window->uiFunc		= uiFunc;

	Window rootWindow;
	rootWindow = DefaultRootWindow( platform->xDisplay );

	XSetWindowAttributes maskAttribute;
	maskAttribute.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;

	window->xWindow = XCreateWindow(
		platform->xDisplay,
		rootWindow,
		x,
		y,
		width,
		height,
		0,
		CopyFromParent,
		InputOutput,
		CopyFromParent,
		CWEventMask,
		&maskAttribute
	);

	XSetWindowAttributes	redirectAttribute;
	redirectAttribute.override_redirect = false;
	XChangeWindowAttributes ( platform->xDisplay, window->xWindow, CWOverrideRedirect, &redirectAttribute );

	XWMHints hints;
	hints.input = true;
	hints.flags = InputHint;
	XSetWMHints( platform->xDisplay, window->xWindow, &hints );

	// make the window visible on the screen
	XMapWindow( platform->xDisplay, window->xWindow );
	XStoreName( platform->xDisplay, window->xWindow, windowTitle );

	// get identifiers for the provided atom name strings
	Atom wm_state;
	wm_state = XInternAtom( platform->xDisplay, "_NET_WM_STATE", false );

	XEvent windowEvent = {};
	//memset ( &xev, 0, sizeof(xev) );
	windowEvent.type					= ClientMessage;
	windowEvent.xclient.window			= window->xWindow;
	windowEvent.xclient.message_type	= wm_state;
	windowEvent.xclient.format			= 32;
	windowEvent.xclient.data.l[0]		= 1;
	windowEvent.xclient.data.l[1]		= false;
	XSendEvent (
		platform->xDisplay,
		rootWindow,
		false,
		SubstructureNotifyMask,
		&windowEvent );

	//esContext->hWnd = (EGLNativeWindowType) win;

	return window;
}

void ImAppPlatformWindowDestroy( ImAppWindow* window )
{

}

bool ImAppPlatformWindowCreateGlContext( ImAppWindow* window )
{
	EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };

	window->eglDisplay = eglGetDisplay( (EGLNativeDisplayType)window->platform->xDisplay );
	if ( window->eglDisplay == EGL_NO_DISPLAY )
	{
		return false;
	}

	// Initialize EGL
	EGLint majorVersion;
	EGLint minorVersion;
	if ( !eglInitialize( window->eglDisplay, &majorVersion, &minorVersion ) )
	{
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}

	// Get configs
	EGLint numConfigs;
	//if ( !eglGetConfigs( window->eglDisplay, NULL, 0, &numConfigs ) )
	//{
	//	ImAppPlatformWindowDestroyGlContext( window );
	//	return false;
	//}

	// Choose config
	EGLint attribList[] =
	{
		EGL_RED_SIZE,			8,
		EGL_GREEN_SIZE,			8,
		EGL_BLUE_SIZE,			8,
		EGL_ALPHA_SIZE,			8,
		EGL_COLOR_BUFFER_TYPE,	EGL_RGB_BUFFER,
		//EGL_ALPHA_SIZE,	 (flags & ES_WINDOW_ALPHA) ? 8 : EGL_DONT_CARE,
		//EGL_SAMPLE_BUFFERS, (flags & ES_WINDOW_MULTISAMPLE) ? 1 : 0,
		EGL_NONE
	};
	//EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };

	EGLConfig config;
	if ( !eglChooseConfig( window->eglDisplay, attribList, &config, 1, &numConfigs ) )
	{
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}

	// Create a surface
	window->eglSurface = eglCreateWindowSurface( window->eglDisplay, config, (EGLNativeWindowType)window->xWindow, NULL );
	if ( window->eglSurface == EGL_NO_SURFACE )
	{
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}

	// Create a GL context
	window->eglContext = eglCreateContext( window->eglDisplay, config, EGL_NO_CONTEXT, contextAttribs );
	if ( window->eglContext == EGL_NO_CONTEXT )
	{
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}	
	
	// Make the context current
	if ( !eglMakeCurrent( window->eglDisplay, window->eglSurface, window->eglSurface, window->eglContext ) )
	{
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}

	// https://github.com/emscripten-core/emscripten/blob/main/test/third_party/glbook/Common/esUtil.c#L128

	return true;
}

void ImAppPlatformWindowDestroyGlContext( ImAppWindow* window )
{
	if( window->eglContext != EGL_NO_CONTEXT )
	{
		eglDestroyContext( window->eglDisplay, window->eglContext );
		window->eglContext = EGL_NO_CONTEXT;
	}

	if( window->eglSurface != EGL_NO_SURFACE )
	{
		eglDestroySurface( window->eglDisplay, window->eglSurface );
		window->eglSurface = EGL_NO_SURFACE;
	}

	window->eglDisplay = EGL_NO_DISPLAY;
}

ImAppWindowDeviceState ImAppPlatformWindowGetGlContextState( const ImAppWindow* window )
{
	if( window->eglContext == EGL_NO_CONTEXT )
	{
		return ImAppWindowDeviceState_NoDevice;
	}

	return ImAppWindowDeviceState_Ok;
}

void ImAppPlatformWindowUpdate( ImAppWindow* window, ImAppPlatformWindowUpdateCallback callback, void* arg )
{
	// TODO
}

bool ImAppPlatformWindowPresent( ImAppWindow* window )
{
	if( window->eglContext == EGL_NO_CONTEXT )
	{
		return false;
	}

	if( !eglSwapBuffers( window->eglDisplay, window->eglSurface ) )
	{
		return false;
	}

	return true;
}

ImAppEventQueue* ImAppPlatformWindowGetEventQueue( ImAppWindow* window )
{
	return &window->eventQueue;
}

ImAppWindowDoUiFunc ImAppPlatformWindowGetUiFunc( ImAppWindow* window )
{
	return window->uiFunc;
}

bool ImAppPlatformWindowPopDropData( ImAppWindow* window, ImAppDropData* outData )
{
	return false;
}

void ImAppPlatformWindowGetViewRect( const ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight )
{
	ImAppPlatformWindowGetPosition( window, outX, outY );
	ImAppPlatformWindowGetSize( window, outWidth, outHeight );
}

bool ImAppPlatformWindowHasFocus( const ImAppWindow* window )
{
	return true;
}

void ImAppPlatformWindowGetSize( const ImAppWindow* window, int* outWidth, int* outHeight )
{
	*outWidth	= window->width;
	*outHeight	= window->height;
}

void ImAppPlatformWindowSetSize( const ImAppWindow* window, int width, int height )
{
	//XEvent resizeEvent;
	//resizeEvent.xresizerequest
	// not supported
}

void ImAppPlatformWindowGetPosition( const ImAppWindow* window, int* outX, int* outY )
{
	*outX = 0;
	*outY = 0;
}

void ImAppPlatformWindowSetPosition( const ImAppWindow* window, int x, int y )
{
	// not supported
}

ImAppWindowState ImAppPlatformWindowGetState( const ImAppWindow* window )
{
	return ImAppWindowState_Maximized;
}

void ImAppPlatformWindowSetState( ImAppWindow* window, ImAppWindowState state )
{
	// not supported
}

const char* ImAppPlatformWindowGetTitle( const ImAppWindow* window )
{
	return NULL;
}

void ImAppPlatformWindowSetTitle( ImAppWindow* window, const char* title )
{
	// not supported
}

void ImAppPlatformWindowSetTitleBounds( ImAppWindow* window, int height, int buttonsX )
{
	// not supported
}

float ImAppPlatformWindowGetDpiScale( const ImAppWindow* window )
{
	return 1.0f;
}

void ImAppPlatformWindowClose( ImAppWindow* window )
{
	// TODO
}

void ImAppPlatformResourceGetPath( ImAppPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName )
{
	// TODO
	outPath[ 0 ] = '\0';
}

ImAppBlob ImAppPlatformResourceLoad( ImAppPlatform* platform, const char* resourceName )
{
	// TODO
	const ImAppBlob result = { NULL, 0 };
	return result;
}

ImAppBlob ImAppPlatformResourceLoadRange( ImAppPlatform* platform, const char* resourceName, uintsize offset, uintsize length )
{
	// TODO
	const ImAppBlob result = { NULL, 0 };
	return result;
}

ImAppFile* ImAppPlatformResourceOpen( ImAppPlatform* platform, const char* resourceName )
{
	// TODO
	return NULL;
}

uintsize ImAppPlatformResourceRead( ImAppFile* file, void* outData, uintsize length, uintsize offset )
{
	// TODO
	return 0;
}

void ImAppPlatformResourceClose( ImAppPlatform* platform, ImAppFile* file )
{
	// TODO
}

ImAppBlob ImAppPlatformResourceLoadSystemFont( ImAppPlatform* platform, const char* fontName )
{
	// TODO
	const ImAppBlob result = { NULL, 0 };
	return result;
}

void ImAppPlatformResourceFree( ImAppPlatform* platform, ImAppBlob blob )
{
	// TODO
}

ImAppFileWatcher* ImAppPlatformFileWatcherCreate( ImAppPlatform* platform )
{
	return NULL;
}

void ImAppPlatformFileWatcherDestroy( ImAppPlatform* platform, ImAppFileWatcher* watcher )
{
}

void ImAppPlatformFileWatcherAddPath( ImAppFileWatcher* watcher, const char* path )
{
}

void ImAppPlatformFileWatcherRemovePath( ImAppFileWatcher* watcher, const char* path )
{
}

bool ImAppPlatformFileWatcherPopEvent( ImAppFileWatcher* watcher, ImAppFileWatchEvent* outEvent )
{
	return false;
}

ImAppThread* ImAppPlatformThreadCreate( ImAppPlatform* platform, const char* name, ImAppThreadFunc func, void* arg )
{
	pthread_attr_t attr;
	if( pthread_attr_init( &attr ) < 0 )
	{
		return NULL;
	}

	if( pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE ) < 0 )
	{
		pthread_attr_destroy( &attr );
		return NULL;
	}

	ImAppThread* thread = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppThread );
	if( !thread )
	{
		return NULL;
	}

	thread->platform	= platform;
	thread->func		= func;
	thread->arg			= arg;

	const int result = pthread_create( &thread->handle, &attr, ImAppPlatformThreadEntry, thread );
	pthread_attr_destroy( &attr );

	if( result < 0 )
	{
		ImUiMemoryFree( platform->allocator, thread );
		return NULL;
	}

	return thread;
}

void ImAppPlatformThreadDestroy( ImAppThread* thread )
{
	pthread_join( thread->handle, NULL );

	ImUiMemoryFree( thread->platform->allocator, thread );
}

bool ImAppPlatformThreadIsRunning( const ImAppThread* thread )
{
	return __atomic_load_n( &thread->hasExit, __ATOMIC_RELAXED ) == 0;
}

static void* ImAppPlatformThreadEntry( void* voidThread )
{
	ImAppThread* thread = (ImAppThread*)voidThread;

	thread->func( thread->arg );

	__atomic_store_n( &thread->hasExit, 1u, __ATOMIC_RELAXED );

	return NULL;
}

ImAppMutex* ImAppPlatformMutexCreate( ImAppPlatform* platform )
{
	pthread_mutex_t* mutex = IMUI_MEMORY_NEW_ZERO( platform->allocator, pthread_mutex_t );
	if( pthread_mutex_init( mutex, NULL ) < 0 )
	{
		ImUiMemoryFree( platform->allocator, mutex );
		return NULL;
	}

	return (ImAppMutex*)mutex;
}

void ImAppPlatformMutexDestroy( ImAppPlatform* platform, ImAppMutex* mutex )
{
	pthread_mutex_destroy( (pthread_mutex_t*)mutex );
	ImUiMemoryFree( platform->allocator, mutex );
}

void ImAppPlatformMutexLock( ImAppMutex* mutex )
{
	pthread_mutex_lock( (pthread_mutex_t*)mutex );
}

void ImAppPlatformMutexUnlock( ImAppMutex* mutex )
{
	pthread_mutex_unlock( (pthread_mutex_t*)mutex );
}

ImAppSemaphore* ImAppPlatformSemaphoreCreate( ImAppPlatform* platform )
{
	sem_t* semaphore = IMUI_MEMORY_NEW_ZERO( platform->allocator, sem_t );
	if( sem_init( semaphore, 0, 0 ) < 0 )
	{
		ImUiMemoryFree( platform->allocator, semaphore );
		return NULL;
	}

	return (ImAppSemaphore*)semaphore;
}

void ImAppPlatformSemaphoreDestroy( ImAppPlatform* platform, ImAppSemaphore* semaphore )
{
	sem_destroy( (sem_t*)semaphore );
	ImUiMemoryFree( platform->allocator, semaphore );
}

void ImAppPlatformSemaphoreInc( ImAppSemaphore* semaphore )
{
	sem_post( (sem_t*)semaphore );
}

bool ImAppPlatformSemaphoreDec( ImAppSemaphore* semaphore, bool wait )
{
	if( wait )
	{
		sem_wait( (sem_t*)semaphore );
		return true;
	}

	return sem_trywait( (sem_t*)semaphore ) == 0;
}

#endif

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

struct ImappPlatform
{
	ImuiAllocator*		allocator;

	Display*			xDisplay;
};

#include "imapp_platform_pthread.h"

struct ImappWindow
{
	ImappPlatform*		platform;
	ImappEventQueue		eventQueue;

	Window				xWindow;
	EGLDisplay			eglDisplay;
	EGLContext			eglContext;
	EGLSurface			eglSurface;

	int					width;
	int					height;

	ImappWindowDoUiFunc uiFunc;
};

int main()
{
	ImappPlatform platform = { 0 };

	const int result = imappMain( &platform, 0, NULL );

	return result;
}

bool imappPlatformInitialize( ImappPlatform* platform, ImuiAllocator* allocator, const char* resourcePath )
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

void imappPlatformShutdown( ImappPlatform* platform )
{
	if( platform->xDisplay )
	{
		//XCloseDisplay( platform->xDisplay );
		platform->xDisplay = NULL;
	}

	platform->allocator = NULL;
}

sint64 imappPlatformTick( ImappPlatform* platform, sint64 lastTickValue, sint64 tickInterval )
{
	return 1;
}

double imappPlatformTicksToSeconds( ImappPlatform* platform, sint64 tickValue )
{
	return 0.01666;
}

void imappPlatformShowError( ImappPlatform* platform, const char* message )
{
	EM_ASM( {
		alert(UTF8ToString($0));
	}, message );
}

void imappPlatformSetMouseCursor( ImappPlatform* platform, ImuiInputMouseCursor cursor )
{
		// TODO
}

void imappPlatformSetClipboardText( ImappPlatform* platform, const char* text )
{
	// TODO
}

void imappPlatformGetClipboardText( ImappPlatform* platform, ImuiContext* imui )
{
	// TODO
}

ImappWindow* imappPlatformWindowCreate( ImappPlatform* platform, const ImappWindowParameters* parameters )
{
	ImappWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImappWindow );

	window->platform	= platform;
	window->width		= parameters->width;
	window->height		= parameters->height;

	Window rootWindow;
	rootWindow = DefaultRootWindow( platform->xDisplay );

	XSetWindowAttributes maskAttribute;
	maskAttribute.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;

	window->xWindow = XCreateWindow(
		platform->xDisplay,
		rootWindow,
		parameters->x,
		parameters->y,
		parameters->width,
		parameters->height,
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
	XStoreName( platform->xDisplay, window->xWindow, parameters->title );

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

	// Ref: https://github.com/emscripten-core/emscripten/blob/main/test/third_party/glbook/Common/esUtil.c#L128
	EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };

	window->eglDisplay = eglGetDisplay( (EGLNativeDisplayType)window->platform->xDisplay );
	if ( window->eglDisplay == EGL_NO_DISPLAY )
	{
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	// Initialize EGL
	EGLint majorVersion;
	EGLint minorVersion;
	if ( !eglInitialize( window->eglDisplay, &majorVersion, &minorVersion ) )
	{
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	// Get configs
	EGLint numConfigs;
	//if ( !eglGetConfigs( window->eglDisplay, NULL, 0, &numConfigs ) )
	//{
	//	imappPlatformWindowDestroyGlContext( window );
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
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	// Create a surface
	window->eglSurface = eglCreateWindowSurface( window->eglDisplay, config, (EGLNativeWindowType)window->xWindow, NULL );
	if ( window->eglSurface == EGL_NO_SURFACE )
	{
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	// Create a GL context
	window->eglContext = eglCreateContext( window->eglDisplay, config, EGL_NO_CONTEXT, contextAttribs );
	if ( window->eglContext == EGL_NO_CONTEXT )
	{
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	// Make the context current
	if ( !eglMakeCurrent( window->eglDisplay, window->eglSurface, window->eglSurface, window->eglContext ) )
	{
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	return window;
}

void imappPlatformWindowDestroy( ImappWindow* window )
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

	// TODO

	imuiMemoryFree( window->platform->allocator, window );
}

ImappWindowDeviceState imappPlatformWindowGetGlContextState( const ImappWindow* window )
{
	if( window->eglContext == EGL_NO_CONTEXT )
	{
		return ImappWindowDeviceState_NoDevice;
	}

	return ImappWindowDeviceState_Ok;
}

void imappPlatformWindowUpdate( ImappWindow* window, imappPlatformWindowUpdateCallback callback, void* arg )
{
	// TODO
}

bool imappPlatformWindowBeginRender( ImappWindow* window )
{
	if( !eglMakeCurrent( window->eglDisplay, window->eglSurface, window->eglSurface, window->eglContext ) )
	{
		IMAPP_DEBUG_LOGE( "Failed to activate GL context." );
		return false;
	}

	return true;
}

bool imappPlatformWindowEndRender( ImappWindow* window )
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

ImappEventQueue* imappPlatformWindowGetEventQueue( ImappWindow* window )
{
	return &window->eventQueue;
}

ImappWindowDoUiFunc imappPlatformWindowGetUiFunc( ImappWindow* window )
{
	return window->uiFunc;
}

bool imappPlatformWindowPopDropData( ImappWindow* window, ImappDropData* outData )
{
	return false;
}

void imappPlatformWindowGetViewRect( const ImappWindow* window, int* outX, int* outY, int* outWidth, int* outHeight )
{
	imappPlatformWindowGetPosition( window, outX, outY );
	imappPlatformWindowGetSize( window, outWidth, outHeight );
}

bool imappPlatformWindowHasFocus( const ImappWindow* window )
{
	return true;
}

void imappPlatformWindowGetSize( const ImappWindow* window, int* outWidth, int* outHeight )
{
	*outWidth	= window->width;
	*outHeight	= window->height;
}

void imappPlatformWindowSetSize( ImappWindow* window, int width, int height )
{
	//XEvent resizeEvent;
	//resizeEvent.xresizerequest
	// not supported
}

void imappPlatformWindowGetPosition( const ImappWindow* window, int* outX, int* outY )
{
	*outX = 0;
	*outY = 0;
}

void imappPlatformWindowSetPosition( const ImappWindow* window, int x, int y )
{
	// not supported
}

ImappWindowStyle imappPlatformWindowGetStyle( const ImappWindow* window )
{
	return ImappWindowStyle_Borderless;
}

ImappWindowState imappPlatformWindowGetState( const ImappWindow* window )
{
	return ImappWindowState_Maximized;
}

void imappPlatformWindowSetState( ImappWindow* window, ImappWindowState state )
{
	// not supported
}

const char* imappPlatformWindowGetTitle( const ImappWindow* window )
{
	return NULL;
}

void imappPlatformWindowSetTitle( ImappWindow* window, const char* title )
{
	// not supported
}

void imappPlatformWindowSetTitleBounds( ImappWindow* window, int height, int buttonsX )
{
	// not supported
}

float imappPlatformWindowGetDpiScale( const ImappWindow* window )
{
	return 1.0f;
}

void imappPlatformWindowClose( ImappWindow* window )
{
	// TODO
}

void imappPlatformResourceGetPath( ImappPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName )
{
	// TODO
	outPath[ 0 ] = '\0';
}

ImappBlob imappPlatformResourceLoad( ImappPlatform* platform, const char* resourceName )
{
	// TODO
	const ImappBlob result = { NULL, 0 };
	return result;
}

ImappBlob imappPlatformResourceLoadRange( ImappPlatform* platform, const char* resourceName, uintsize offset, uintsize length )
{
	// TODO
	const ImappBlob result = { NULL, 0 };
	return result;
}

ImappFile* imappPlatformResourceOpen( ImappPlatform* platform, const char* resourceName )
{
	// TODO
	return NULL;
}

uintsize imappPlatformResourceRead( ImappFile* file, void* outData, uintsize length, uintsize offset )
{
	// TODO
	return 0;
}

void imappPlatformResourceClose( ImappPlatform* platform, ImappFile* file )
{
	// TODO
}

ImappBlob imappPlatformResourceLoadSystemFont( ImappPlatform* platform, const char* fontName )
{
	// TODO
	const ImappBlob result = { NULL, 0 };
	return result;
}

void imappPlatformResourceFree( ImappPlatform* platform, ImappBlob blob )
{
	// TODO
}

ImappFileWatcher* imappPlatformFileWatcherCreate( ImappPlatform* platform )
{
	return NULL;
}

void imappPlatformFileWatcherDestroy( ImappPlatform* platform, ImappFileWatcher* watcher )
{
}

void imappPlatformFileWatcherAddPath( ImappFileWatcher* watcher, const char* path )
{
}

void imappPlatformFileWatcherRemovePath( ImappFileWatcher* watcher, const char* path )
{
}

bool imappPlatformFileWatcherPopEvent( ImappFileWatcher* watcher, ImappFileWatchEvent* outEvent )
{
	return false;
}

#endif

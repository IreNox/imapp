#include "../imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )

#include <android/native_activity.h>

#include <EGL/egl.h>
#include <dlfcn.h>
#include <errno.h>
#include <jni.h>
#include <pthread.h>
#include <unistd.h>

//////////////////////////////////////////////////////////////////////////
// Main

typedef struct ImAppPlatform
{
	ANativeActivity*		pActivity;

	int						eventPipeRead;
	int						eventPipeWrite;

	pthread_mutex_t			threadConditionMutex;
	pthread_cond_t			threadCondition;
	pthread_t				thread;

	ALooper*				pLooper;
	AInputQueue*			pInputQueue;

	int						viewTop;
	int						viewHeight;

	bool					started;
	bool					stoped;
} ImAppPlatform;

typedef enum ImAppAndroidLooperType
{
	ImAppAndroidLooperType_Invalid,
	ImAppAndroidLooperType_Event,
	ImAppAndroidLooperType_Input
} ImAppAndroidLooperType;

typedef enum ImAppAndroidEventType
{
	ImAppAndroidEventType_State,
	ImAppAndroidEventType_Window,
	ImAppAndroidEventType_Input,
	ImAppAndroidEventType_Shutdown
} ImAppAndroidEventType;

typedef enum ImAppAndroidStateType
{
	ImAppAndroidStateType_Start,
	ImAppAndroidStateType_Pause,
	ImAppAndroidStateType_Resume,
	ImAppAndroidStateType_Stop
} ImAppAndroidStateType;

typedef union ImAppAndroidEventData
{
	struct
	{
		ImAppAndroidStateType	state;
	} state;
	struct
	{
		ANativeWindow*			pWindow;
	} window;
	struct
	{
		AInputQueue*			pInputQueue;
	} input;
} ImAppAndroidEventData;

typedef struct ImAppAndroidEvent
{
	ImAppAndroidEventType		type;
	ImAppAndroidEventData		data;
} ImAppAndroidEvent;

static void*	ImAppAndroidCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize );
static void		ImAppAndroidDestroy( ImAppPlatform* pPlatform );

static void*	ImAppAndroidThreadEntryPoint( void* pArgument );

static void		ImAppAndroidGetViewBounds( ImAppPlatform* pPlatform );

static bool		ImAppAndroidEventPop( ImAppAndroidEvent* pTarget, ImAppPlatform* pPlatform );
static void		ImAppAndroidEventPush( ImAppPlatform* pPlatform, const ImAppAndroidEvent* pEvent );

static void		ImAppAndroidOnStart( ANativeActivity* pActivity );
static void		ImAppAndroidOnPause( ANativeActivity* pActivity );
static void		ImAppAndroidOnResume( ANativeActivity* pActivity );
static void		ImAppAndroidOnStop( ANativeActivity* pActivity );
static void		ImAppAndroidOnDestroy( ANativeActivity* pActivity );
static void		ImAppAndroidOnLowMemory( ANativeActivity* pActivity );
static void*	ImAppAndroidOnSaveInstanceState( ANativeActivity* pActivity, size_t* pOutLen );
static void		ImAppAndroidOnConfigurationChanged( ANativeActivity* pActivity );
static void		ImAppAndroidOnWindowFocusChanged( ANativeActivity* pActivity, int focused );
static void		ImAppAndroidOnNativeWindowCreated( ANativeActivity* pActivity, ANativeWindow* pWindow );
static void		ImAppAndroidOnNativeWindowDestroyed( ANativeActivity* pActivity, ANativeWindow* pWindow );
static void		ImAppAndroidOnInputQueueCreated( ANativeActivity* pActivity, AInputQueue* pInputQueue );
static void		ImAppAndroidOnInputQueueDestroyed( ANativeActivity* pActivity, AInputQueue* pInputQueue );

void ImAppShowError( ImAppPlatform* pPlatform, const char* pMessage )
{

}

void ANativeActivity_onCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize )
{
	ImAppTrace( "Creating: %p\n", pActivity );

	pActivity->callbacks->onStart					= ImAppAndroidOnStart;
	pActivity->callbacks->onPause					= ImAppAndroidOnPause;
	pActivity->callbacks->onResume					= ImAppAndroidOnResume;
	pActivity->callbacks->onStop					= ImAppAndroidOnStop;
	pActivity->callbacks->onDestroy					= ImAppAndroidOnDestroy;
	pActivity->callbacks->onLowMemory				= ImAppAndroidOnLowMemory;
	pActivity->callbacks->onSaveInstanceState		= ImAppAndroidOnSaveInstanceState;
	pActivity->callbacks->onConfigurationChanged	= ImAppAndroidOnConfigurationChanged;
	pActivity->callbacks->onWindowFocusChanged		= ImAppAndroidOnWindowFocusChanged;
	pActivity->callbacks->onNativeWindowCreated		= ImAppAndroidOnNativeWindowCreated;
	pActivity->callbacks->onNativeWindowDestroyed	= ImAppAndroidOnNativeWindowDestroyed;
	pActivity->callbacks->onInputQueueCreated		= ImAppAndroidOnInputQueueCreated;
	pActivity->callbacks->onInputQueueDestroyed		= ImAppAndroidOnInputQueueDestroyed;

	pActivity->instance = ImAppAndroidCreate( pActivity, savedState, savedStateSize );
}

static void* ImAppAndroidCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize )
{
	ImAppPlatform* pPlatform = IMAPP_NEW_ZERO( ImAppPlatform );

	pPlatform->pActivity = pActivity;

	pthread_mutex_init( &pPlatform->threadConditionMutex, NULL );
	pthread_cond_init( &pPlatform->threadCondition, NULL );

	int eventPipe[ 2u ];
	if( pipe( eventPipe ) )
	{
		ImAppTrace( "Could not create pipe: %s\n", strerror( errno ) );
		ImAppFree( pPlatform );
		return NULL;
	}
	pPlatform->eventPipeRead	= eventPipe[ 0u ];
	pPlatform->eventPipeWrite	= eventPipe[ 1u ];

	pthread_attr_t threadAttributes;
	pthread_attr_init( &threadAttributes );
	pthread_attr_setdetachstate( &threadAttributes, PTHREAD_CREATE_DETACHED );
	pthread_create( &pPlatform->thread, &threadAttributes, ImAppAndroidThreadEntryPoint, pPlatform );

	return pPlatform;
}

static void ImAppAndroidDestroy( ImAppPlatform* pPlatform )
{
	{
		ImAppAndroidEvent androidEvent;
		androidEvent.type = ImAppAndroidEventType_Shutdown;
		ImAppAndroidEventPush( pPlatform, &androidEvent );
	}

	pthread_mutex_lock( &pPlatform->threadConditionMutex );
	while( !pPlatform->stoped )
	{
		pthread_cond_wait( &pPlatform->threadCondition, &pPlatform->threadConditionMutex );
	}
	pthread_mutex_unlock( &pPlatform->threadConditionMutex );

	pthread_cond_destroy( &pPlatform->threadCondition );
	pthread_mutex_destroy( &pPlatform->threadConditionMutex );

	close( pPlatform->eventPipeWrite );
	close( pPlatform->eventPipeRead );

	ImAppFree( pPlatform );
}

static void* ImAppAndroidThreadEntryPoint( void* pArgument )
{
	ImAppPlatform* pPlatform = (ImAppPlatform*)pArgument;

	pPlatform->pLooper = ALooper_prepare( ALOOPER_PREPARE_ALLOW_NON_CALLBACKS );
	ALooper_addFd( pPlatform->pLooper, pPlatform->eventPipeRead, ImAppAndroidLooperType_Event, ALOOPER_EVENT_INPUT, NULL, pPlatform );

	pthread_mutex_lock( &pPlatform->threadConditionMutex );
	pPlatform->started = true;
	pthread_cond_broadcast( &pPlatform->threadCondition );
	pthread_mutex_unlock( &pPlatform->threadConditionMutex );

	ImAppMain( pPlatform, 0, NULL );

	pthread_mutex_lock( &pPlatform->threadConditionMutex );
	pPlatform->stoped = true;
	pthread_cond_broadcast( &pPlatform->threadCondition );
	pthread_mutex_unlock( &pPlatform->threadConditionMutex );

	return NULL;
}

static void ImAppAndroidGetViewBounds( ImAppPlatform* pPlatform )
{
	JNIEnv* pEnv = NULL;
	(*pPlatform->pActivity->vm)->AttachCurrentThread( pPlatform->pActivity->vm, &pEnv, NULL );

	jclass activityClass = (*pEnv)->GetObjectClass( pEnv, pPlatform->pActivity->clazz );
	jmethodID getWindowMethod = (*pEnv)->GetMethodID( pEnv, activityClass, "getWindow", "()Landroid/view/Window;" );

	jobject window = (*pEnv)->CallObjectMethod( pEnv, pPlatform->pActivity->clazz, getWindowMethod );

	jclass windowClass = (*pEnv)->GetObjectClass( pEnv, window );
	jmethodID getDecorViewMethod = (*pEnv)->GetMethodID( pEnv, windowClass, "getDecorView", "()Landroid/view/View;" );

	jobject view = (*pEnv)->CallObjectMethod( pEnv, window, getDecorViewMethod );

	jclass viewClass = (*pEnv)->GetObjectClass( pEnv, view );
	jmethodID getWindowVisibleDisplayFrameMethod = (*pEnv)->GetMethodID( pEnv, viewClass, "getWindowVisibleDisplayFrame", "(Landroid/graphics/Rect;)V" );

	jclass rectClass = (*pEnv)->FindClass( pEnv, "android/graphics/Rect" );
	jmethodID rectConstructor = (*pEnv)->GetMethodID( pEnv, rectClass, "<init>", "()V" );
	jfieldID topField = (*pEnv)->GetFieldID( pEnv, rectClass, "top", "I" );
	jfieldID bottomField = (*pEnv)->GetFieldID( pEnv, rectClass, "bottom", "I" );

	jobject rect = (*pEnv)->NewObject( pEnv, rectClass, rectConstructor );

	(*pEnv)->CallVoidMethod( pEnv, view, getWindowVisibleDisplayFrameMethod, rect );

	const int viewBottom = (*pEnv)->GetIntField( pEnv, rect, bottomField );
	pPlatform->viewTop		= (*pEnv)->GetIntField( pEnv, rect, topField );
	pPlatform->viewHeight	= viewBottom - pPlatform->viewTop;

	(*pPlatform->pActivity->vm)->DetachCurrentThread( pPlatform->pActivity->vm );
}

static bool ImAppAndroidEventPop( ImAppAndroidEvent* pTarget, ImAppPlatform* pPlatform )
{
	const int bytesRead = read( pPlatform->eventPipeRead, pTarget, sizeof( *pTarget ) );
	IMAPP_ASSERT( bytesRead == 0 || bytesRead == sizeof( *pTarget ) );
	return bytesRead == sizeof( *pTarget );
}

static void ImAppAndroidEventPush( ImAppPlatform* pPlatform, const ImAppAndroidEvent* pEvent )
{
	if( write( pPlatform->eventPipeWrite, pEvent, sizeof( *pEvent ) ) != sizeof( *pEvent ) )
	{
		ImAppTrace( "Write to pipe failed. Error: %s\n", strerror( errno ) );
	}
}

static void ImAppAndroidOnStart( ANativeActivity* pActivity )
{
	ImAppTrace( "Start: %p\n", pActivity );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_State, .data.state.state = ImAppAndroidStateType_Start };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnPause( ANativeActivity* pActivity )
{
	ImAppTrace( "Pause: %p\n", pActivity );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_State, .data.state.state = ImAppAndroidStateType_Pause };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnResume( ANativeActivity* pActivity )
{
	ImAppTrace( "Resume: %p\n", pActivity );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_State, .data.state.state = ImAppAndroidStateType_Resume };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnStop( ANativeActivity* pActivity )
{
	ImAppTrace( "Stop: %p\n", pActivity );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_State, .data.state.state = ImAppAndroidStateType_Stop };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnDestroy( ANativeActivity* pActivity )
{
	ImAppTrace( "Destroy: %p\n", pActivity );

	ImAppPlatform* pPlatform = (ImAppPlatform*)pActivity->instance;
	ImAppAndroidDestroy( pPlatform );
}

static void ImAppAndroidOnLowMemory( ANativeActivity* pActivity )
{
	ImAppTrace( "LowMemory: %p\n", pActivity );
	//android_app_write_cmd( (struct android_app*)pActivity->instance, APP_CMD_LOW_MEMORY );
}

static void* ImAppAndroidOnSaveInstanceState( ANativeActivity* pActivity, size_t* pOutLen )
{
	ImAppTrace( "SaveInstanceState: %p\n", pActivity );
	*pOutLen = 0u;
	return NULL;
}

static void ImAppAndroidOnConfigurationChanged( ANativeActivity* pActivity )
{
	ImAppTrace( "ConfigurationChanged: %p\n", pActivity );
	//android_app_write_cmd( (struct android_app*)pActivity->instance, APP_CMD_CONFIG_CHANGED );
}

static void ImAppAndroidOnWindowFocusChanged( ANativeActivity* pActivity, int focused )
{
	ImAppTrace( "WindowFocusChanged: %p -- %d\n", pActivity, focused );
	//android_app_write_cmd( (struct android_app*)pActivity->instance, focused ? APP_CMD_GAINED_FOCUS : APP_CMD_LOST_FOCUS );
}

static void ImAppAndroidOnNativeWindowCreated( ANativeActivity* pActivity, ANativeWindow* pWindow )
{
	ImAppTrace( "NativeWindowCreated: %p -- %p\n", pActivity, pWindow );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_Window, .data.window.pWindow = pWindow };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnNativeWindowDestroyed( ANativeActivity* pActivity, ANativeWindow* pWindow )
{
	ImAppTrace( "NativeWindowDestroyed: %p -- %p\n", pActivity, pWindow );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_Window, .data.window.pWindow = NULL };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnInputQueueCreated( ANativeActivity* pActivity, AInputQueue* pInputQueue )
{
	ImAppTrace( "InputQueueCreated: %p -- %p\n", pActivity, pInputQueue );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_Input, .data.input.pInputQueue = pInputQueue };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnInputQueueDestroyed( ANativeActivity* pActivity, AInputQueue* pInputQueue )
{
	ImAppTrace( "InputQueueDestroyed: %p -- %p\n", pActivity, pInputQueue );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_Input, .data.input.pInputQueue = NULL };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

//////////////////////////////////////////////////////////////////////////
// Shared Libraries

ImAppSharedLibHandle ImAppSharedLibOpen( const char* pSharedLibName )
{
	const ImAppSharedLibHandle handle = (ImAppSharedLibHandle)dlopen( pSharedLibName, RTLD_NOW | RTLD_LOCAL );
	if( handle == NULL )
	{
		const char* pErrorText = dlerror();
		ImAppTrace( "Failed loading shared lib '%s'. Error: %s", pSharedLibName, pErrorText );
		return NULL;
	}

	return handle;
}

void ImAppSharedLibClose( ImAppSharedLibHandle libHandle )
{
	dlclose( libHandle );
}

void* ImAppSharedLibGetFunction( ImAppSharedLibHandle libHandle, const char* pFunctionName )
{
	return dlsym( libHandle, pFunctionName );
}

//////////////////////////////////////////////////////////////////////////
// Window

struct ImAppWindow
{
	ImAppPlatform*	pPlatform;
	ANativeWindow*	pNativeWindow;

	bool			isOpen;
	bool			hasDeviceChange;

	EGLDisplay		display;
	EGLSurface		surface;
	EGLContext		context;

	int				width;
	int				height;
};

static bool	ImAppWindowCreateContext( ImAppWindow* pWindow, ANativeWindow* pNativeWindow );
static void	ImAppWindowDestroyContext( ImAppWindow* pWindow );

static void	ImAppWindowHandleEvent( ImAppWindow* pWindow, const ImAppAndroidEvent* pEvent );

ImAppWindow* ImAppWindowCreate( ImAppPlatform* pPlatform, const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state )
{
	ImAppWindow* pWindow = IMAPP_NEW_ZERO( ImAppWindow );
	if( pWindow == NULL )
	{
		return NULL;
	}

	pWindow->pPlatform	= pPlatform;
	pWindow->isOpen		= true;
	pWindow->display	= EGL_NO_DISPLAY;
	pWindow->surface	= EGL_NO_SURFACE;
	pWindow->context	= EGL_NO_CONTEXT;

	while( pWindow->isOpen &&
		pWindow->pNativeWindow == NULL )
	{
		ImAppWindowTick( pWindow, 0, 0 );
	}

	if( pWindow->pNativeWindow == NULL )
	{
		ImAppWindowDestroy( pWindow );
		return NULL;
	}

	pWindow->hasDeviceChange = false;

	return pWindow;
}

void ImAppWindowDestroy( ImAppWindow* pWindow )
{
	ImAppWindowDestroyContext( pWindow );

	ImAppFree( pWindow );
}

static bool	ImAppWindowCreateContext( ImAppWindow* pWindow, ANativeWindow* pNativeWindow )
{
	pWindow->display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	eglInitialize( pWindow->display, NULL, NULL );

	const EGLint displayAttributes[] = {
		EGL_SURFACE_TYPE,	EGL_WINDOW_BIT,
		EGL_BLUE_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_RED_SIZE,		8,
		EGL_NONE
	};

	EGLint configCount;
	EGLConfig config;
	eglChooseConfig( pWindow->display, displayAttributes, &config, 1, &configCount );

	EGLint format;
	eglGetConfigAttrib( pWindow->display, config, EGL_NATIVE_VISUAL_ID, &format );

	ANativeWindow_setBuffersGeometry( pNativeWindow, 0, 0, format );

	pWindow->surface = eglCreateWindowSurface( pWindow->display, config, pNativeWindow, NULL );

	const EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION,	2,
		EGL_NONE
	};

	pWindow->context = eglCreateContext( pWindow->display, config, NULL, contextAttributes );

	if( eglMakeCurrent( pWindow->display, pWindow->surface, pWindow->surface, pWindow->context ) == EGL_FALSE )
	{
		return false;
	}

	eglQuerySurface( pWindow->display, pWindow->surface, EGL_WIDTH, &pWindow->width );
	eglQuerySurface( pWindow->display, pWindow->surface, EGL_HEIGHT, &pWindow->height );

	pWindow->pNativeWindow = pNativeWindow;

	ImAppAndroidGetViewBounds( pWindow->pPlatform );

	return true;
}

static void	ImAppWindowDestroyContext( ImAppWindow* pWindow )
{
	if( pWindow->display == EGL_NO_DISPLAY )
	{
		return;
	}

	eglMakeCurrent( pWindow->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );

	if( pWindow->context != EGL_NO_CONTEXT )
	{
		eglDestroyContext( pWindow->display, pWindow->context );
		pWindow->context = EGL_NO_CONTEXT;
	}

	if( pWindow->surface != EGL_NO_SURFACE )
	{
		eglDestroySurface( pWindow->display, pWindow->surface );
		pWindow->surface = EGL_NO_SURFACE;
	}

	eglTerminate( pWindow->display );
	pWindow->display = EGL_NO_DISPLAY;

	pWindow->pNativeWindow = NULL;
}

int64_t ImAppWindowTick( ImAppWindow* pWindow, int64_t lastTickValue, int64_t tickInterval )
{
	struct timespec currentTime;
	clock_gettime( CLOCK_REALTIME, &currentTime );
	const int64_t currentTickValue	= (currentTime.tv_sec * 1000) + (currentTime.tv_nsec / 100000);
	const int64_t lastTickDuration	= currentTickValue - lastTickValue;
	const int64_t waitDuration		= tickInterval - (lastTickDuration < tickInterval ? lastTickDuration : tickInterval);

	int timeoutValue = tickInterval == 0 ? -1 : (int)waitDuration;
	int eventType;
	void* pUserData;
	while( ALooper_pollAll( timeoutValue, NULL, &eventType, &pUserData ) >= 0 )
	{
		timeoutValue = 0;

		if( eventType == ImAppAndroidLooperType_Event )
		{
			ImAppAndroidEvent androidEvent;
			if( ImAppAndroidEventPop( &androidEvent, pWindow->pPlatform ) )
			{
				ImAppWindowHandleEvent( pWindow, &androidEvent );
			}
		}
		else if( eventType == ImAppAndroidLooperType_Input )
		{

		}
	}

	return currentTickValue;
}

static void ImAppWindowHandleEvent( ImAppWindow* pWindow, const ImAppAndroidEvent* pEvent )
{
	switch( pEvent->type )
	{
	case ImAppAndroidEventType_Window:
		ImAppWindowDestroyContext( pWindow );
		pWindow->isOpen = ImAppWindowCreateContext( pWindow, pEvent->data.window.pWindow );
		pWindow->hasDeviceChange = true;
		break;

	case ImAppAndroidLooperType_Input:
		pWindow->pPlatform->pInputQueue = pEvent->data.input.pInputQueue;
		break;

	default:
		break;
	}
}

bool ImAppWindowPresent( ImAppWindow* pWindow )
{
	if( pWindow->hasDeviceChange )
	{
		pWindow->hasDeviceChange = false;
		return false;
	}

	eglSwapBuffers( pWindow->display, pWindow->surface );
	return true;
}

bool ImAppWindowIsOpen( ImAppWindow* pWindow )
{
	return pWindow->isOpen;
}

void ImAppWindowGetViewRect( int* pX, int* pY, int* pWidth, int* pHeight, ImAppWindow* pWindow )
{
	*pX			= 0u;
	*pY			= pWindow->pPlatform->viewTop;
	*pWidth		= pWindow->width;
	*pHeight	= pWindow->pPlatform->viewHeight;
}

void ImAppWindowGetSize( int* pWidth, int* pHeight, ImAppWindow* pWindow )
{
	*pWidth		= pWindow->width;
	*pHeight	= pWindow->height;
}

void ImAppWindowGetPosition( int* pX, int* pY, ImAppWindow* pWindow )
{
	*pX	= 0u;
	*pY	= 0u;
}

ImAppWindowState ImAppWindowGetState( ImAppWindow* pWindow )
{
	return ImAppWindowState_Maximized;
}

//////////////////////////////////////////////////////////////////////////
// Input

struct ImAppInput
{

};

ImAppInput* ImAppInputCreate( ImAppPlatform* pPlatform, ImAppWindow* pWindow )
{
	ImAppInput* pInput = IMAPP_NEW_ZERO( ImAppInput );

	return pInput;
}

void ImAppInputDestroy( ImAppInput* pInput )
{
	ImAppFree( pInput );
}

void ImAppInputApply( ImAppInput* pInput, struct nk_context* pNkContext )
{

}

#endif

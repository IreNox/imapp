#include "../imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )

#include <android/asset_manager.h>
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
	int						viewLeft;
	int						viewWidth;
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
	ImAppAndroidEventType_StateChanged,
	ImAppAndroidEventType_WindowChanged,
	ImAppAndroidEventType_WindowResize,
	ImAppAndroidEventType_InputChanged,
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
static void		ImAppAndroidOnNativeWindowResized( ANativeActivity* pActivity, ANativeWindow* pWindow );
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
	pActivity->callbacks->onNativeWindowResized		= ImAppAndroidOnNativeWindowResized;
	pActivity->callbacks->onInputQueueCreated		= ImAppAndroidOnInputQueueCreated;
	pActivity->callbacks->onInputQueueDestroyed		= ImAppAndroidOnInputQueueDestroyed;

	pActivity->instance = ImAppAndroidCreate( pActivity, savedState, savedStateSize );
}

static void* ImAppAndroidCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize )
{
	ImAppPlatform* pPlatform = IMAPP_NEW_ZERO( ImAppAllocatorGetDefault(), ImAppPlatform );

	pPlatform->pActivity = pActivity;

	pthread_mutex_init( &pPlatform->threadConditionMutex, NULL );
	pthread_cond_init( &pPlatform->threadCondition, NULL );

	int eventPipe[ 2u ];
	if( pipe( eventPipe ) )
	{
		ImAppTrace( "Could not create pipe: %s\n", strerror( errno ) );
		ImAppFree( ImAppAllocatorGetDefault(), pPlatform );
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

	ImAppFree( ImAppAllocatorGetDefault(), pPlatform );
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

	ANativeActivity_finish( pPlatform->pActivity );

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
	jfieldID leftField = (*pEnv)->GetFieldID( pEnv, rectClass, "left", "I" );
	jfieldID rightField = (*pEnv)->GetFieldID( pEnv, rectClass, "right", "I" );
	jfieldID bottomField = (*pEnv)->GetFieldID( pEnv, rectClass, "bottom", "I" );

	jobject rect = (*pEnv)->NewObject( pEnv, rectClass, rectConstructor );

	(*pEnv)->CallVoidMethod( pEnv, view, getWindowVisibleDisplayFrameMethod, rect );

	const int viewRight = (*pEnv)->GetIntField( pEnv, rect, rightField );
	const int viewBottom = (*pEnv)->GetIntField( pEnv, rect, bottomField );
	pPlatform->viewTop		= (*pEnv)->GetIntField( pEnv, rect, topField );
	pPlatform->viewLeft		= (*pEnv)->GetIntField( pEnv, rect, leftField );
	pPlatform->viewWidth	= viewRight - pPlatform->viewLeft;
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

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_StateChanged, .data.state.state = ImAppAndroidStateType_Start };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnPause( ANativeActivity* pActivity )
{
	ImAppTrace( "Pause: %p\n", pActivity );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_StateChanged, .data.state.state = ImAppAndroidStateType_Pause };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnResume( ANativeActivity* pActivity )
{
	ImAppTrace( "Resume: %p\n", pActivity );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_StateChanged, .data.state.state = ImAppAndroidStateType_Resume };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnStop( ANativeActivity* pActivity )
{
	ImAppTrace( "Stop: %p\n", pActivity );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_StateChanged, .data.state.state = ImAppAndroidStateType_Stop };
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

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_WindowChanged, .data.window.pWindow = pWindow };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnNativeWindowDestroyed( ANativeActivity* pActivity, ANativeWindow* pWindow )
{
	ImAppTrace( "NativeWindowDestroyed: %p -- %p\n", pActivity, pWindow );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_WindowChanged, .data.window.pWindow = NULL };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnNativeWindowResized( ANativeActivity* pActivity, ANativeWindow* pWindow )
{
	ImAppTrace( "NativeWindowResized: %p -- %p\n", pActivity, pWindow );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_WindowResize, .data.window.pWindow = pWindow };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnInputQueueCreated( ANativeActivity* pActivity, AInputQueue* pInputQueue )
{
	ImAppTrace( "InputQueueCreated: %p -- %p\n", pActivity, pInputQueue );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_InputChanged, .data.input.pInputQueue = pInputQueue };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnInputQueueDestroyed( ANativeActivity* pActivity, AInputQueue* pInputQueue )
{
	ImAppTrace( "InputQueueDestroyed: %p -- %p\n", pActivity, pInputQueue );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_InputChanged, .data.input.pInputQueue = NULL };
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
	ImAppAllocator*		pAllocator;
	ImAppPlatform*		pPlatform;
	ImAppEventQueue*	pEventQueue;

	ANativeWindow*		pNativeWindow;
	bool				isOpen;

	EGLDisplay			display;
	EGLSurface			surface;
	EGLContext			context;
	bool				hasDeviceChange;

	int					width;
	int					height;
};

static void	ImAppWindowHandleWindowChangedEvent( ImAppWindow* pWindow, const ImAppAndroidEvent* pSystemEvent );
static void	ImAppWindowHandleWindowResizeEvent( ImAppWindow* pWindow, const ImAppAndroidEvent* pSystemEvent );
static void	ImAppWindowHandleInputChangedEvent( ImAppWindow* pWindow, const ImAppAndroidEvent* pSystemEvent );

static bool	ImAppWindowHandleInputEvent( ImAppWindow* pWindow, const AInputEvent* pInputEvent );

ImAppWindow* ImAppWindowCreate( ImAppAllocator* pAllocator, ImAppPlatform* pPlatform, const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state )
{
	ImAppWindow* pWindow = IMAPP_NEW_ZERO( pAllocator, ImAppWindow );
	if( pWindow == NULL )
	{
		return NULL;
	}

	pWindow->pAllocator		= pAllocator;
	pWindow->pPlatform		= pPlatform;
	pWindow->pEventQueue	= ImAppEventQueueCreate( pAllocator );
	pWindow->isOpen			= true;
	pWindow->display		= EGL_NO_DISPLAY;
	pWindow->surface		= EGL_NO_SURFACE;
	pWindow->context		= EGL_NO_CONTEXT;

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
	IMAPP_ASSERT( pWindow->context == EGL_NO_CONTEXT );

	if( pWindow->pEventQueue != NULL )
	{
		ImAppEventQueueDestroy( pWindow->pEventQueue );
		pWindow->pEventQueue = NULL;
	}

	ImAppFree( pWindow->pAllocator, pWindow );
}

bool ImAppWindowCreateGlContext( ImAppWindow* pWindow )
{
	IMAPP_ASSERT( pWindow->pNativeWindow != NULL );
	IMAPP_ASSERT( pWindow->display == EGL_NO_DISPLAY );

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

	ANativeWindow_setBuffersGeometry( pWindow->pNativeWindow, 0, 0, format );

	pWindow->surface = eglCreateWindowSurface( pWindow->display, config, pWindow->pNativeWindow, NULL );

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

	ImAppAndroidGetViewBounds( pWindow->pPlatform );

	return true;
}

void ImAppWindowDestroyGlContext( ImAppWindow* pWindow )
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
	int waitDuration;
	int64_t currentTickValue;
	{
		struct timespec currentTime;
		clock_gettime( CLOCK_REALTIME, &currentTime );

		const int64_t currentSeconds		= currentTime.tv_sec;
		const int64_t currentNanoSeconds	= currentTime.tv_nsec;
		currentTickValue					= (currentSeconds * 1000ll) + (currentNanoSeconds / 1000000ll);
		const int64_t lastTickDuration		= currentTickValue - lastTickValue;
		waitDuration						= (int)(tickInterval - (lastTickDuration < tickInterval ? lastTickDuration : tickInterval));
		IMAPP_ASSERT( waitDuration <= tickInterval );
	}

	int timeoutValue = tickInterval == 0 ? -1 : (int)waitDuration;
	int eventCount;
	int eventType;
	void* pUserData;
	while( (eventType = ALooper_pollAll( timeoutValue, NULL, &eventCount, &pUserData )) >= 0 )
	{
		IMAPP_ASSERT( pUserData == pWindow->pPlatform );
		timeoutValue = 0;

		if( eventType == ImAppAndroidLooperType_Event )
		{
			ImAppAndroidEvent androidEvent;
			if( ImAppAndroidEventPop( &androidEvent, pWindow->pPlatform ) )
			{
				switch( androidEvent.type )
				{
				case ImAppAndroidEventType_WindowChanged:
					ImAppWindowHandleWindowChangedEvent( pWindow, &androidEvent );
					break;

				case ImAppAndroidEventType_WindowResize:
					ImAppWindowHandleWindowResizeEvent( pWindow, &androidEvent );
					break;

				case ImAppAndroidEventType_InputChanged:
					ImAppWindowHandleInputChangedEvent( pWindow, &androidEvent );
					break;

				default:
					break;
				}
			}
		}
		else if( eventType == ImAppAndroidLooperType_Input )
		{
			AInputEvent* pInputEvent = NULL;
			while( AInputQueue_getEvent( pWindow->pPlatform->pInputQueue, &pInputEvent ) >= 0 )
			{
				if( AInputQueue_preDispatchEvent( pWindow->pPlatform->pInputQueue, pInputEvent ) )
				{
					continue;
				}

				const bool  handled = ImAppWindowHandleInputEvent( pWindow, pInputEvent );
				AInputQueue_finishEvent( pWindow->pPlatform->pInputQueue, pInputEvent, handled );
			}
		}
	}

	return currentTickValue;
}

static void ImAppWindowHandleWindowChangedEvent( ImAppWindow* pWindow, const ImAppAndroidEvent* pSystemEvent )
{
	if( pWindow->context == EGL_NO_CONTEXT )
	{
		pWindow->pNativeWindow = pSystemEvent->data.window.pWindow;
		return;
	}

	ImAppWindowDestroyGlContext( pWindow );
	pWindow->pNativeWindow = pSystemEvent->data.window.pWindow;
	pWindow->isOpen = ImAppWindowCreateGlContext( pWindow );
	pWindow->hasDeviceChange = true;
}

static void	ImAppWindowHandleWindowResizeEvent( ImAppWindow* pWindow, const ImAppAndroidEvent* pSystemEvent )
{
	IMAPP_ASSERT( pWindow->pNativeWindow == pSystemEvent->data.window.pWindow );

	pWindow->width	= ANativeWindow_getWidth( pWindow->pNativeWindow );
	pWindow->height	= ANativeWindow_getHeight( pWindow->pNativeWindow );

	ImAppAndroidGetViewBounds( pWindow->pPlatform );
}

static void ImAppWindowHandleInputChangedEvent( ImAppWindow* pWindow, const ImAppAndroidEvent* pSystemEvent )
{
	if( pWindow->pPlatform->pInputQueue != NULL )
	{
		AInputQueue_detachLooper( pWindow->pPlatform->pInputQueue );
	}

	pWindow->pPlatform->pInputQueue = pSystemEvent->data.input.pInputQueue;

	if( pWindow->pPlatform->pInputQueue != NULL )
	{
		AInputQueue_attachLooper( pWindow->pPlatform->pInputQueue, pWindow->pPlatform->pLooper, ImAppAndroidLooperType_Input, NULL, pWindow->pPlatform );
	}
}

static bool	ImAppWindowHandleInputEvent( ImAppWindow* pWindow, const AInputEvent* pInputEvent )
{
	const uint32_t eventType = AInputEvent_getType( pInputEvent );
	switch( eventType )
	{
	case AINPUT_EVENT_TYPE_KEY:
		{
			// not implemented
			const uint32_t keyAction = AKeyEvent_getAction( pInputEvent );
			switch( keyAction )
			{
			case AKEY_EVENT_ACTION_DOWN:
			case AKEY_EVENT_ACTION_UP:
				{
					//const int32_t scanCode = AKeyEvent_getScanCode( pInputEvent );
					//const enum nk_keys nkKey = s_inputKeyMapping[ scanCode ];
					//if( nkKey != NK_KEY_NONE )
					//{
					//	nk_input_key( pNkContext, nkKey, pKeyEvent->type == SDL_KEYDOWN );
					//}

					//if( pKeyEvent->state == SDL_PRESSED &&
					//	pKeyEvent->keysym.sym >= ' ' &&
					//	pKeyEvent->keysym.sym <= '~' )
					//{
					//	nk_input_char( pNkContext, (char)pKeyEvent->keysym.sym );
					//}

					//const nk_bool down = keyAction == AKEY_EVENT_ACTION_DOWN;
					//nk_input_key( pNkContext, ..., down );
				}
				break;

			case AKEY_EVENT_ACTION_MULTIPLE:
				break;

			default:
				break;
			}
		}
		break;

	case AINPUT_EVENT_TYPE_MOTION:
		{
			const uint32_t motionAction = AMotionEvent_getAction( pInputEvent );
			switch( motionAction )
			{
			case AMOTION_EVENT_ACTION_DOWN:
			case AMOTION_EVENT_ACTION_UP:
			case AMOTION_EVENT_ACTION_CANCEL:
			case AMOTION_EVENT_ACTION_MOVE:
				{
					const int x = (int)AMotionEvent_getX( pInputEvent, 0 );
					const int y = (int)AMotionEvent_getY( pInputEvent, 0 );

					const ImAppEvent motionEvent = { .motion = { .type = ImAppEventType_Motion, .x = x, .y = y } };
					ImAppEventQueuePush( pWindow->pEventQueue, &motionEvent );

					if( motionAction != AMOTION_EVENT_ACTION_MOVE )
					{
						const ImAppEventType eventType	= motionAction == AMOTION_EVENT_ACTION_DOWN ? ImAppEventType_ButtonDown : ImAppEventType_ButtonUp;
						const ImAppEvent buttonEvent	= { .button = { .type = eventType, .x = x, .y = y, .button = ImAppInputButton_Left, .repeateCount = 1 } };
						ImAppEventQueuePush( pWindow->pEventQueue, &buttonEvent );
					}

					return true;
				}
				break;

			default:
				break;
			}
		}
		break;

	default:
		break;
	}

	return false;
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

ImAppEventQueue* ImAppWindowGetEventQueue( ImAppWindow* pWindow )
{
	return pWindow->pEventQueue;
}

void ImAppWindowGetViewRect( int* pX, int* pY, int* pWidth, int* pHeight, ImAppWindow* pWindow )
{
	*pX			= 0u; //pWindow->pPlatform->viewLeft;
	*pY			= pWindow->pPlatform->viewTop;
	*pWidth		= pWindow->pPlatform->viewWidth;
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
// Resources

ImAppResource ImAppResourceLoad( ImAppPlatform* pPlatform, ImAppAllocator* pAllocator, const char* pResourceName )
{
	AAsset* pAsset = AAssetManager_open( pPlatform->pActivity->assetManager, pResourceName, AASSET_MODE_BUFFER );
	if( pAsset == NULL )
	{
		const ImAppResource result = { NULL, 0u };
		return result;
	}

	const size_t size = AAsset_getLength64( pAsset );
	void* pData = ImAppMalloc( pAllocator, size );

	const void* pSourceData = AAsset_getBuffer( pAsset );
	memcpy( pData, pSourceData, size );

	AAsset_close( pAsset );

	const ImAppResource result = { pData, size };
	return result;
}

#endif

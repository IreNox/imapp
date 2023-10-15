#include "../imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )

#include "../imapp_debug.h"
#include "../imapp_event_queue.h"

#include <android/asset_manager.h>
#include <android/native_activity.h>

#include <EGL/egl.h>
#include <dlfcn.h>
#include <errno.h>
#include <jni.h>
#include <pthread.h>
#include <string.h>
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

	ImUiAllocator*			allocator;
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
		ANativeWindow*			window;
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
static void		ImAppAndroidDestroy( ImAppPlatform* platform );

static void*	ImAppAndroidThreadEntryPoint( void* pArgument );

static void		ImAppAndroidGetViewBounds( ImAppPlatform* platform );

static bool		ImAppAndroidEventPop( ImAppAndroidEvent* pTarget, ImAppPlatform* platform );
static void		ImAppAndroidEventPush( ImAppPlatform* platform, const ImAppAndroidEvent* pEvent );

static void		ImAppAndroidOnStart( ANativeActivity* pActivity );
static void		ImAppAndroidOnPause( ANativeActivity* pActivity );
static void		ImAppAndroidOnResume( ANativeActivity* pActivity );
static void		ImAppAndroidOnStop( ANativeActivity* pActivity );
static void		ImAppAndroidOnDestroy( ANativeActivity* pActivity );
static void		ImAppAndroidOnLowMemory( ANativeActivity* pActivity );
static void*	ImAppAndroidOnSaveInstanceState( ANativeActivity* pActivity, size_t* pOutLen );
static void		ImAppAndroidOnConfigurationChanged( ANativeActivity* pActivity );
static void		ImAppAndroidOnWindowFocusChanged( ANativeActivity* pActivity, int focused );
static void		ImAppAndroidOnNativeWindowCreated( ANativeActivity* pActivity, ANativeWindow* window );
static void		ImAppAndroidOnNativeWindowDestroyed( ANativeActivity* pActivity, ANativeWindow* window );
static void		ImAppAndroidOnNativeWindowResized( ANativeActivity* pActivity, ANativeWindow* window );
static void		ImAppAndroidOnInputQueueCreated( ANativeActivity* pActivity, AInputQueue* pInputQueue );
static void		ImAppAndroidOnInputQueueDestroyed( ANativeActivity* pActivity, AInputQueue* pInputQueue );

void ImAppPlatformShowError( ImAppPlatform* platform, const char* message )
{
}

void ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor )
{
}

void ANativeActivity_onCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize )
{
	IMAPP_DEBUG_LOGI( "Creating: %p\n", pActivity );

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
	ImAppPlatform* platform = (ImAppPlatform*)malloc( sizeof( *platform ) );

	platform->pActivity = pActivity;

	pthread_mutex_init( &platform->threadConditionMutex, NULL );
	pthread_cond_init( &platform->threadCondition, NULL );

	int eventPipe[ 2u ];
	if( pipe( eventPipe ) )
	{
		IMAPP_DEBUG_LOGE( "Could not create pipe: %s\n", strerror( errno ) );
		free( platform );
		return NULL;
	}
	platform->eventPipeRead	= eventPipe[ 0u ];
	platform->eventPipeWrite	= eventPipe[ 1u ];

	pthread_attr_t threadAttributes;
	pthread_attr_init( &threadAttributes );
	pthread_attr_setdetachstate( &threadAttributes, PTHREAD_CREATE_DETACHED );
	pthread_create( &platform->thread, &threadAttributes, ImAppAndroidThreadEntryPoint, platform );

	return platform;
}

static void ImAppAndroidDestroy( ImAppPlatform* platform )
{
	{
		ImAppAndroidEvent androidEvent;
		androidEvent.type = ImAppAndroidEventType_Shutdown;
		ImAppAndroidEventPush( platform, &androidEvent );
	}

	pthread_mutex_lock( &platform->threadConditionMutex );
	while( !platform->stoped )
	{
		pthread_cond_wait( &platform->threadCondition, &platform->threadConditionMutex );
	}
	pthread_mutex_unlock( &platform->threadConditionMutex );

	pthread_cond_destroy( &platform->threadCondition );
	pthread_mutex_destroy( &platform->threadConditionMutex );

	close( platform->eventPipeWrite );
	close( platform->eventPipeRead );

	free( platform );
}

static void* ImAppAndroidThreadEntryPoint( void* pArgument )
{
	ImAppPlatform* platform = (ImAppPlatform*)pArgument;

	platform->pLooper = ALooper_prepare( ALOOPER_PREPARE_ALLOW_NON_CALLBACKS );
	ALooper_addFd( platform->pLooper, platform->eventPipeRead, ImAppAndroidLooperType_Event, ALOOPER_EVENT_INPUT, NULL, platform );

	pthread_mutex_lock( &platform->threadConditionMutex );
	platform->started = true;
	pthread_cond_broadcast( &platform->threadCondition );
	pthread_mutex_unlock( &platform->threadConditionMutex );

	ImAppMain( platform, 0, NULL );

	pthread_mutex_lock( &platform->threadConditionMutex );
	platform->stoped = true;
	pthread_cond_broadcast( &platform->threadCondition );
	pthread_mutex_unlock( &platform->threadConditionMutex );

	ANativeActivity_finish( platform->pActivity );

	return NULL;
}

static void ImAppAndroidGetViewBounds( ImAppPlatform* platform )
{
	JNIEnv* pEnv = NULL;
	(*platform->pActivity->vm)->AttachCurrentThread( platform->pActivity->vm, &pEnv, NULL );

	jclass activityClass = (*pEnv)->GetObjectClass( pEnv, platform->pActivity->clazz );
	jmethodID getWindowMethod = (*pEnv)->GetMethodID( pEnv, activityClass, "getWindow", "()Landroid/view/Window;" );

	jobject window = (*pEnv)->CallObjectMethod( pEnv, platform->pActivity->clazz, getWindowMethod );

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
	platform->viewTop		= (*pEnv)->GetIntField( pEnv, rect, topField );
	platform->viewLeft		= (*pEnv)->GetIntField( pEnv, rect, leftField );
	platform->viewWidth	= viewRight - platform->viewLeft;
	platform->viewHeight	= viewBottom - platform->viewTop;

	(*platform->pActivity->vm)->DetachCurrentThread( platform->pActivity->vm );
}

static bool ImAppAndroidEventPop( ImAppAndroidEvent* pTarget, ImAppPlatform* platform )
{
	const int bytesRead = read( platform->eventPipeRead, pTarget, sizeof( *pTarget ) );
	IMAPP_ASSERT( bytesRead == 0 || bytesRead == sizeof( *pTarget ) );
	return bytesRead == sizeof( *pTarget );
}

static void ImAppAndroidEventPush( ImAppPlatform* platform, const ImAppAndroidEvent* pEvent )
{
	if( write( platform->eventPipeWrite, pEvent, sizeof( *pEvent ) ) != sizeof( *pEvent ) )
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

	ImAppPlatform* platform = (ImAppPlatform*)pActivity->instance;
	ImAppAndroidDestroy( platform );
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

static void ImAppAndroidOnNativeWindowCreated( ANativeActivity* pActivity, ANativeWindow* window )
{
	ImAppTrace( "NativeWindowCreated: %p -- %p\n", pActivity, window );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_WindowChanged, .data.window.window = window };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnNativeWindowDestroyed( ANativeActivity* pActivity, ANativeWindow* window )
{
	ImAppTrace( "NativeWindowDestroyed: %p -- %p\n", pActivity, window );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_WindowChanged, .data.window.window = NULL };
	ImAppAndroidEventPush( (ImAppPlatform*)pActivity->instance, &androidEvent );
}

static void ImAppAndroidOnNativeWindowResized( ANativeActivity* pActivity, ANativeWindow* window )
{
	ImAppTrace( "NativeWindowResized: %p -- %p\n", pActivity, window );

	const ImAppAndroidEvent androidEvent = { ImAppAndroidEventType_WindowResize, .data.window.window = window };
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
// Platform

bool ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	return true;
}

void ImAppPlatformShutdown( ImAppPlatform* platform )
{
	platform->allocator = NULL;
}

//////////////////////////////////////////////////////////////////////////
// Window

struct ImAppWindow
{
	ImUiAllocator*		allocator;
	ImAppPlatform*		platform;
	ImAppEventQueue*	eventQueue;

	ANativeWindow*		pNativeWindow;
	bool				isOpen;

	EGLDisplay			display;
	EGLSurface			surface;
	EGLContext			context;
	bool				hasDeviceChange;

	int					width;
	int					height;
};

static void	ImAppPlatformWindowHandleWindowChangedEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent );
static void	ImAppPlatformWindowHandleWindowResizeEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent );
static void	ImAppPlatformWindowHandleInputChangedEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent );

static bool	ImAppPlatformWindowHandleInputEvent( ImAppWindow* window, const AInputEvent* pInputEvent );

ImAppWindow* ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state )
{
	ImAppWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppWindow );
	if( window == NULL )
	{
		return NULL;
	}

	window->allocator	= platform->allocator;
	window->platform	= platform;
	window->eventQueue	= ImAppEventQueueCreate( platform->allocator );
	window->isOpen		= true;
	window->display		= EGL_NO_DISPLAY;
	window->surface		= EGL_NO_SURFACE;
	window->context		= EGL_NO_CONTEXT;

	while( window->isOpen &&
		   window->pNativeWindow == NULL )
	{
		ImAppPlatformWindowTick( window, 0, 0 );
	}

	if( window->pNativeWindow == NULL )
	{
		ImAppPlatformWindowDestroy( window );
		return NULL;
	}

	window->hasDeviceChange = false;

	return window;
}

void ImAppPlatformWindowDestroy( ImAppWindow* window )
{
	IMAPP_ASSERT( window->context == EGL_NO_CONTEXT );

	if( window->eventQueue != NULL )
	{
		ImAppEventQueueDestroy( window->eventQueue );
		window->eventQueue = NULL;
	}

	ImUiMemoryFree( window->allocator, window );
}

bool ImAppPlatformWindowCreateGlContext( ImAppWindow* window )
{
	IMAPP_ASSERT( window->pNativeWindow != NULL );
	IMAPP_ASSERT( window->display == EGL_NO_DISPLAY );

	window->display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	eglInitialize( window->display, NULL, NULL );

	const EGLint displayAttributes[] = {
		EGL_SURFACE_TYPE,	EGL_WINDOW_BIT,
		EGL_BLUE_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_RED_SIZE,		8,
		EGL_NONE
	};

	EGLint configCount;
	EGLConfig config;
	eglChooseConfig( window->display, displayAttributes, &config, 1, &configCount );

	EGLint format;
	eglGetConfigAttrib( window->display, config, EGL_NATIVE_VISUAL_ID, &format );

	ANativeWindow_setBuffersGeometry( window->pNativeWindow, 0, 0, format );

	window->surface = eglCreateWindowSurface( window->display, config, window->pNativeWindow, NULL );

	const EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION,	2,
		EGL_NONE
	};

	window->context = eglCreateContext( window->display, config, NULL, contextAttributes );

	if( eglMakeCurrent( window->display, window->surface, window->surface, window->context ) == EGL_FALSE )
	{
		return false;
	}

	eglQuerySurface( window->display, window->surface, EGL_WIDTH, &window->width );
	eglQuerySurface( window->display, window->surface, EGL_HEIGHT, &window->height );

	ImAppAndroidGetViewBounds( window->platform );

	return true;
}

void ImAppPlatformWindowDestroyGlContext( ImAppWindow* window )
{
	if( window->display == EGL_NO_DISPLAY )
	{
		return;
	}

	eglMakeCurrent( window->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );

	if( window->context != EGL_NO_CONTEXT )
	{
		eglDestroyContext( window->display, window->context );
		window->context = EGL_NO_CONTEXT;
	}

	if( window->surface != EGL_NO_SURFACE )
	{
		eglDestroySurface( window->display, window->surface );
		window->surface = EGL_NO_SURFACE;
	}

	eglTerminate( window->display );
	window->display = EGL_NO_DISPLAY;

	window->pNativeWindow = NULL;
}

int64_t ImAppPlatformWindowTick( ImAppWindow* window, int64_t lastTickValue, int64_t tickInterval )
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
		IMAPP_ASSERT( pUserData == window->platform );
		timeoutValue = 0;

		if( eventType == ImAppAndroidLooperType_Event )
		{
			ImAppAndroidEvent androidEvent;
			if( ImAppAndroidEventPop( &androidEvent, window->platform ) )
			{
				switch( androidEvent.type )
				{
				case ImAppAndroidEventType_WindowChanged:
					ImAppPlatformWindowHandleWindowChangedEvent( window, &androidEvent );
					break;

				case ImAppAndroidEventType_WindowResize:
					ImAppPlatformWindowHandleWindowResizeEvent( window, &androidEvent );
					break;

				case ImAppAndroidEventType_InputChanged:
					ImAppPlatformWindowHandleInputChangedEvent( window, &androidEvent );
					break;

				default:
					break;
				}
			}
		}
		else if( eventType == ImAppAndroidLooperType_Input )
		{
			AInputEvent* pInputEvent = NULL;
			while( AInputQueue_getEvent( window->platform->pInputQueue, &pInputEvent ) >= 0 )
			{
				if( AInputQueue_preDispatchEvent( window->platform->pInputQueue, pInputEvent ) )
				{
					continue;
				}

				const bool  handled = ImAppPlatformWindowHandleInputEvent( window, pInputEvent );
				AInputQueue_finishEvent( window->platform->pInputQueue, pInputEvent, handled );
			}
		}
	}

	return currentTickValue;
}

static void ImAppPlatformWindowHandleWindowChangedEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent )
{
	if( window->context == EGL_NO_CONTEXT )
	{
		window->pNativeWindow = pSystemEvent->data.window.window;
		return;
	}

	ImAppPlatformWindowDestroyGlContext( window );
	window->pNativeWindow = pSystemEvent->data.window.window;
	window->isOpen = ImAppPlatformWindowCreateGlContext( window );
	window->hasDeviceChange = true;
}

static void	ImAppPlatformWindowHandleWindowResizeEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent )
{
	IMAPP_ASSERT( window->pNativeWindow == pSystemEvent->data.window.window );

	window->width	= ANativeWindow_getWidth( window->pNativeWindow );
	window->height	= ANativeWindow_getHeight( window->pNativeWindow );

	ImAppAndroidGetViewBounds( window->platform );
}

static void ImAppPlatformWindowHandleInputChangedEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent )
{
	if( window->platform->pInputQueue != NULL )
	{
		AInputQueue_detachLooper( window->platform->pInputQueue );
	}

	window->platform->pInputQueue = pSystemEvent->data.input.pInputQueue;

	if( window->platform->pInputQueue != NULL )
	{
		AInputQueue_attachLooper( window->platform->pInputQueue, window->platform->pLooper, ImAppAndroidLooperType_Input, NULL, window->platform );
	}
}

static bool	ImAppPlatformWindowHandleInputEvent( ImAppWindow* window, const AInputEvent* pInputEvent )
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
					ImAppEventQueuePush( window->eventQueue, &motionEvent );

					if( motionAction != AMOTION_EVENT_ACTION_MOVE )
					{
						const ImAppEventType eventType	= motionAction == AMOTION_EVENT_ACTION_DOWN ? ImAppEventType_ButtonDown : ImAppEventType_ButtonUp;
						const ImAppEvent buttonEvent	= { .button = { .type = eventType, .x = x, .y = y, .button = ImUiInputMouseButton_Left, .repeateCount = 1 } };
						ImAppEventQueuePush( window->eventQueue, &buttonEvent );
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

bool ImAppPlatformWindowPresent( ImAppWindow* window )
{
	if( window->hasDeviceChange )
	{
		window->hasDeviceChange = false;
		return false;
	}

	eglSwapBuffers( window->display, window->surface );
	return true;
}

ImAppEventQueue* ImAppPlatformWindowGetEventQueue( ImAppWindow* window )
{
	return window->eventQueue;
}

void ImAppPlatformWindowGetViewRect( int* pX, int* pY, int* pWidth, int* pHeight, ImAppWindow* window )
{
	*pX			= 0u; //window->platform->viewLeft;
	*pY			= window->platform->viewTop;
	*pWidth		= window->platform->viewWidth;
	*pHeight	= window->platform->viewHeight;
}

void ImAppPlatformWindowGetSize( int* pWidth, int* pHeight, ImAppWindow* window )
{
	*pWidth		= window->width;
	*pHeight	= window->height;
}

void ImAppPlatformWindowGetPosition( int* pX, int* pY, ImAppWindow* window )
{
	*pX	= 0u;
	*pY	= 0u;
}

ImAppWindowState ImAppPlatformWindowGetState( ImAppWindow* window )
{
	return ImAppWindowState_Maximized;
}

//////////////////////////////////////////////////////////////////////////
// Resources

ImAppBlob ImAppPlatformResourceLoad( ImAppPlatform* platform, ImUiStringView resourceName )
{
	AAsset* asset = AAssetManager_open( platform->pActivity->assetManager, resourceName.data, AASSET_MODE_BUFFER );
	if( asset == NULL )
	{
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	const size_t size = AAsset_getLength64( asset );
	void* data = ImUiMemoryAlloc( platform->allocator, size );

	const void* sourceData = AAsset_getBuffer( asset );
	memcpy( data, sourceData, size );

	AAsset_close( asset );

	const ImAppBlob result = { data, size };
	return result;
}

ImAppBlob ImAppPlatformResourceLoadSystemFont( ImAppPlatform* platform, ImUiStringView resourceName )
{
	const ImAppBlob result = { NULL, 0u };
	return result;
}

#endif

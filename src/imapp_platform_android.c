#include "imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )

#include "imapp_debug.h"
#include "imapp_internal.h"
#include "imapp_event_queue.h"

#include <android/asset_manager.h>
#include <android/native_activity.h>

#include <EGL/egl.h>
#include <dlfcn.h>
#include <errno.h>
#include <jni.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
	uint8					inputKeyMapping[ 161u ];
    uint32					inputKeyRepeat[ ImUiInputKey_MAX ];

	int						viewTop;
	int						viewLeft;
	int						viewWidth;
	int						viewHeight;
	float					dpiScale;

	bool					started;
	bool					stoped;

	ImUiAllocator*			allocator;
} ImAppPlatform;

#include "imapp_platform_pthread.h"

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

static void		ImAppAndroidUpdateViewBoundsAndDpiScale( ImAppPlatform* platform );

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

void imappPlatformShowError( ImAppPlatform* platform, const char* message )
{
}

void imappPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor )
{
}

void imappPlatformSetClipboardText( ImAppPlatform* platform, const char* text )
{
	JNIEnv* pEnv = NULL;
	(*platform->pActivity->vm)->AttachCurrentThread( platform->pActivity->vm, &pEnv, NULL );

	jclass activityClass = (*pEnv)->GetObjectClass( pEnv, platform->pActivity->clazz );
	jmethodID getSystemServiceMethod = (*pEnv)->GetMethodID( pEnv, activityClass, "getSystemService", "()Ljava/lang/Object;" );

	jobject clipboardManager = (*pEnv)->CallObjectMethod( pEnv, platform->pActivity->clazz, getSystemServiceMethod, "clipboard" );

	jclass clipboardManagerClass = (*pEnv)->GetObjectClass( pEnv, clipboardManager );
	jmethodID setTextMethod = (*pEnv)->GetMethodID( pEnv, clipboardManagerClass, "setText", "(Ljava/lang/CharSequence;)V" );

	jstring textString = (*pEnv)->NewStringUTF( pEnv, text );
	(*pEnv)->CallVoidMethod( pEnv, clipboardManager, setTextMethod, textString );

	(*platform->pActivity->vm)->DetachCurrentThread( platform->pActivity->vm );
}

void imappPlatformGetClipboardText( ImAppPlatform* platform, ImUiContext* imui )
{
	JNIEnv* pEnv = NULL;
	(*platform->pActivity->vm)->AttachCurrentThread( platform->pActivity->vm, &pEnv, NULL );

	jclass activityClass = (*pEnv)->GetObjectClass( pEnv, platform->pActivity->clazz );
	jmethodID getSystemServiceMethod = (*pEnv)->GetMethodID( pEnv, activityClass, "getSystemService", "()Ljava/lang/Object;" );

	jobject clipboardManager = (*pEnv)->CallObjectMethod( pEnv, platform->pActivity->clazz, getSystemServiceMethod, "clipboard" );

	jclass clipboardManagerClass = (*pEnv)->GetObjectClass( pEnv, clipboardManager );
	jmethodID getTextMethod = (*pEnv)->GetMethodID( pEnv, clipboardManagerClass, "getText", "()Ljava/lang/CharSequence;" );

	jstring textString = (*pEnv)->CallObjectMethod( pEnv, clipboardManager, getTextMethod );

	const char* text = (*pEnv)->GetStringUTFChars( pEnv, textString, NULL );
	ImUiInputSetPasteText( imui, text );

	(*platform->pActivity->vm)->DetachCurrentThread( platform->pActivity->vm );
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
    memset( platform, 0, sizeof( *platform ) );

	platform->pActivity = pActivity;

#if IMAPP_ENABLED( IMAPP_DEBUG )
	uintsize maxScanCode = 0u;
#endif
	for( uintsize i = 0u; i < ImUiInputKey_MAX; ++i )
	{
		const ImUiInputKey keyValue = (ImUiInputKey)i;

		uint16 scanCode = 0u;
		switch( keyValue )
		{
		case ImUiInputKey_None:				break;
		case ImUiInputKey_A:				scanCode = AKEYCODE_A; break;
		case ImUiInputKey_B:				scanCode = AKEYCODE_B; break;
		case ImUiInputKey_C:				scanCode = AKEYCODE_C; break;
		case ImUiInputKey_D:				scanCode = AKEYCODE_D; break;
		case ImUiInputKey_E:				scanCode = AKEYCODE_E; break;
		case ImUiInputKey_F:				scanCode = AKEYCODE_F; break;
		case ImUiInputKey_G:				scanCode = AKEYCODE_G; break;
		case ImUiInputKey_H:				scanCode = AKEYCODE_H; break;
		case ImUiInputKey_I:				scanCode = AKEYCODE_I; break;
		case ImUiInputKey_J:				scanCode = AKEYCODE_J; break;
		case ImUiInputKey_K:				scanCode = AKEYCODE_K; break;
		case ImUiInputKey_L:				scanCode = AKEYCODE_L; break;
		case ImUiInputKey_M:				scanCode = AKEYCODE_M; break;
		case ImUiInputKey_N:				scanCode = AKEYCODE_N; break;
		case ImUiInputKey_O:				scanCode = AKEYCODE_O; break;
		case ImUiInputKey_P:				scanCode = AKEYCODE_P; break;
		case ImUiInputKey_Q:				scanCode = AKEYCODE_Q; break;
		case ImUiInputKey_R:				scanCode = AKEYCODE_R; break;
		case ImUiInputKey_S:				scanCode = AKEYCODE_S; break;
		case ImUiInputKey_T:				scanCode = AKEYCODE_T; break;
		case ImUiInputKey_U:				scanCode = AKEYCODE_U; break;
		case ImUiInputKey_V:				scanCode = AKEYCODE_V; break;
		case ImUiInputKey_W:				scanCode = AKEYCODE_W; break;
		case ImUiInputKey_X:				scanCode = AKEYCODE_X; break;
		case ImUiInputKey_Y:				scanCode = AKEYCODE_Y; break;
		case ImUiInputKey_Z:				scanCode = AKEYCODE_Z; break;
		case ImUiInputKey_1:				scanCode = AKEYCODE_1; break;
		case ImUiInputKey_2:				scanCode = AKEYCODE_2; break;
		case ImUiInputKey_3:				scanCode = AKEYCODE_3; break;
		case ImUiInputKey_4:				scanCode = AKEYCODE_4; break;
		case ImUiInputKey_5:				scanCode = AKEYCODE_5; break;
		case ImUiInputKey_6:				scanCode = AKEYCODE_6; break;
		case ImUiInputKey_7:				scanCode = AKEYCODE_7; break;
		case ImUiInputKey_8:				scanCode = AKEYCODE_8; break;
		case ImUiInputKey_9:				scanCode = AKEYCODE_9; break;
		case ImUiInputKey_0:				scanCode = AKEYCODE_0; break;
		case ImUiInputKey_Enter:			scanCode = AKEYCODE_ENTER; break;
		case ImUiInputKey_Escape:			scanCode = AKEYCODE_ESCAPE; break;
		case ImUiInputKey_Backspace:		scanCode = AKEYCODE_DEL; break;
		case ImUiInputKey_Tab:				scanCode = AKEYCODE_TAB; break;
		case ImUiInputKey_Space:			scanCode = AKEYCODE_SPACE; break;
		case ImUiInputKey_LeftShift:		scanCode = AKEYCODE_SHIFT_LEFT; break;
		case ImUiInputKey_RightShift:		scanCode = AKEYCODE_SHIFT_RIGHT; break;
		case ImUiInputKey_LeftControl:		scanCode = AKEYCODE_CTRL_LEFT; break;
		case ImUiInputKey_RightControl:		scanCode = AKEYCODE_CTRL_RIGHT; break;
		case ImUiInputKey_LeftAlt:			scanCode = AKEYCODE_ALT_LEFT; break;
		case ImUiInputKey_RightAlt:			scanCode = AKEYCODE_ALT_RIGHT; break;
		case ImUiInputKey_Minus:			scanCode = AKEYCODE_MINUS; break;
		case ImUiInputKey_Equals:			scanCode = AKEYCODE_EQUALS; break;
		case ImUiInputKey_LeftBracket:		scanCode = AKEYCODE_LEFT_BRACKET; break;
		case ImUiInputKey_RightBracket:		scanCode = AKEYCODE_RIGHT_BRACKET; break;
		case ImUiInputKey_Backslash:		scanCode = AKEYCODE_BACKSLASH; break;
		case ImUiInputKey_Semicolon:		scanCode = AKEYCODE_SEMICOLON; break;
		case ImUiInputKey_Apostrophe:		scanCode = AKEYCODE_APOSTROPHE; break;
		case ImUiInputKey_Grave:			scanCode = AKEYCODE_GRAVE; break;
		case ImUiInputKey_Comma:			scanCode = AKEYCODE_COMMA; break;
		case ImUiInputKey_Period:			scanCode = AKEYCODE_PERIOD; break;
		case ImUiInputKey_Slash:			scanCode = AKEYCODE_SLASH; break;
		case ImUiInputKey_F1:				scanCode = AKEYCODE_F1; break;
		case ImUiInputKey_F2:				scanCode = AKEYCODE_F2; break;
		case ImUiInputKey_F3:				scanCode = AKEYCODE_F3; break;
		case ImUiInputKey_F4:				scanCode = AKEYCODE_F4; break;
		case ImUiInputKey_F5:				scanCode = AKEYCODE_F5; break;
		case ImUiInputKey_F6:				scanCode = AKEYCODE_F6; break;
		case ImUiInputKey_F7:				scanCode = AKEYCODE_F7; break;
		case ImUiInputKey_F8:				scanCode = AKEYCODE_F8; break;
		case ImUiInputKey_F9:				scanCode = AKEYCODE_F9; break;
		case ImUiInputKey_F10:				scanCode = AKEYCODE_F10; break;
		case ImUiInputKey_F11:				scanCode = AKEYCODE_F11; break;
		case ImUiInputKey_F12:				scanCode = AKEYCODE_F12; break;
		case ImUiInputKey_Print:			scanCode = AKEYCODE_SYSRQ; break;
		case ImUiInputKey_Pause:			scanCode = AKEYCODE_BREAK; break;
		case ImUiInputKey_Insert:			scanCode = AKEYCODE_INSERT; break;
		case ImUiInputKey_Delete:			scanCode = AKEYCODE_FORWARD_DEL; break;
		case ImUiInputKey_Home:				scanCode = AKEYCODE_MOVE_HOME; break;
		case ImUiInputKey_End:				scanCode = AKEYCODE_MOVE_END; break;
		case ImUiInputKey_PageUp:			scanCode = AKEYCODE_PAGE_UP; break;
		case ImUiInputKey_PageDown:			scanCode = AKEYCODE_PAGE_DOWN; break;
		case ImUiInputKey_Up:				scanCode = AKEYCODE_DPAD_UP; break;
		case ImUiInputKey_Left:				scanCode = AKEYCODE_DPAD_LEFT; break;
		case ImUiInputKey_Down:				scanCode = AKEYCODE_DPAD_DOWN; break;
		case ImUiInputKey_Right:			scanCode = AKEYCODE_DPAD_RIGHT; break;
		case ImUiInputKey_Numpad_Divide:	scanCode = AKEYCODE_NUMPAD_DIVIDE; break;
		case ImUiInputKey_Numpad_Multiply:	scanCode = AKEYCODE_NUMPAD_MULTIPLY; break;
		case ImUiInputKey_Numpad_Minus:		scanCode = AKEYCODE_NUMPAD_SUBTRACT; break;
		case ImUiInputKey_Numpad_Plus:		scanCode = AKEYCODE_NUMPAD_ADD; break;
		case ImUiInputKey_Numpad_Enter:		scanCode = AKEYCODE_NUMPAD_ENTER; break;
		case ImUiInputKey_Numpad_1:			scanCode = AKEYCODE_NUMPAD_1; break;
		case ImUiInputKey_Numpad_2:			scanCode = AKEYCODE_NUMPAD_2; break;
		case ImUiInputKey_Numpad_3:			scanCode = AKEYCODE_NUMPAD_3; break;
		case ImUiInputKey_Numpad_4:			scanCode = AKEYCODE_NUMPAD_4; break;
		case ImUiInputKey_Numpad_5:			scanCode = AKEYCODE_NUMPAD_5; break;
		case ImUiInputKey_Numpad_6:			scanCode = AKEYCODE_NUMPAD_6; break;
		case ImUiInputKey_Numpad_7:			scanCode = AKEYCODE_NUMPAD_7; break;
		case ImUiInputKey_Numpad_8:			scanCode = AKEYCODE_NUMPAD_8; break;
		case ImUiInputKey_Numpad_9:			scanCode = AKEYCODE_NUMPAD_9; break;
		case ImUiInputKey_Numpad_0:			scanCode = AKEYCODE_NUMPAD_0; break;
		case ImUiInputKey_Numpad_Period:	scanCode = AKEYCODE_NUMPAD_DOT; break;
		case ImUiInputKey_MAX:				break;
		}

#if IMAPP_ENABLED( IMAPP_DEBUG )
		maxScanCode = scanCode > maxScanCode ? scanCode : maxScanCode;
#endif

		IMAPP_ASSERT( scanCode < IMAPP_ARRAY_COUNT( platform->inputKeyMapping ) );
		platform->inputKeyMapping[ scanCode ] = keyValue;
	}
	IMAPP_ASSERT( maxScanCode + 1u == IMAPP_ARRAY_COUNT( platform->inputKeyMapping ) );

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

	imappMain( platform, 0, NULL );

	pthread_mutex_lock( &platform->threadConditionMutex );
	platform->stoped = true;
	pthread_cond_broadcast( &platform->threadCondition );
	pthread_mutex_unlock( &platform->threadConditionMutex );

	ANativeActivity_finish( platform->pActivity );

	return NULL;
}

static void ImAppAndroidUpdateViewBoundsAndDpiScale( ImAppPlatform* platform )
{
	JNIEnv* pEnv = NULL;
	(*platform->pActivity->vm)->AttachCurrentThread( platform->pActivity->vm, &pEnv, NULL );

	jclass activityClass = (*pEnv)->GetObjectClass( pEnv, platform->pActivity->clazz );

	{
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
		platform->viewWidth		= viewRight - platform->viewLeft;
		platform->viewHeight	= viewBottom - platform->viewTop;
	}

	{
		jmethodID getWindowManagerMethod = (*pEnv)->GetMethodID( pEnv, activityClass, "getWindowManager", "()Landroid/view/WindowManager;" );

		jobject windowManager = (*pEnv)->CallObjectMethod( pEnv, platform->pActivity->clazz, getWindowManagerMethod );

		jclass windowManagerClass = (*pEnv)->GetObjectClass( pEnv, windowManager );
		jmethodID getCurrentWindowMetricsMethod = NULL;//(*pEnv)->GetMethodID( pEnv, windowManagerClass, "getCurrentWindowMetrics ", "()Landroid/view/WindowMetrics;" );
		if( getCurrentWindowMetricsMethod )
		{
			jobject metrics = (*pEnv)->CallObjectMethod( pEnv, windowManager, getCurrentWindowMetricsMethod );

			jclass metricsClass = (*pEnv)->GetObjectClass( pEnv, metrics );
			jmethodID getDensityMethod = (*pEnv)->GetMethodID( pEnv, metricsClass, "getDensity", "()F" );

			platform->dpiScale = (*pEnv)->CallFloatMethod( pEnv, metrics, getDensityMethod );
		}
		else
		{
			jmethodID getDisplayMethod = (*pEnv)->GetMethodID( pEnv, activityClass, "getDisplay", "()Landroid/view/Display;" );

			jobject display = (*pEnv)->CallObjectMethod( pEnv, platform->pActivity->clazz, getDisplayMethod );

			jclass displayClass = (*pEnv)->GetObjectClass( pEnv, display );
			jmethodID getRealMetricsMethod = (*pEnv)->GetMethodID( pEnv, displayClass, "getRealMetrics", "(Landroid/util/DisplayMetrics;)V" );

			jclass displayMetricsClass = (*pEnv)->FindClass( pEnv, "android/util/DisplayMetrics" );
			jmethodID displayMetricsConstructor = (*pEnv)->GetMethodID( pEnv, displayMetricsClass, "<init>", "()V" );
			jfieldID densityField = (*pEnv)->GetFieldID( pEnv, displayMetricsClass, "density", "F" );

			jobject displayMetrics = (*pEnv)->NewObject( pEnv, displayMetricsClass, displayMetricsConstructor );

			(*pEnv)->CallVoidMethod( pEnv, display, getRealMetricsMethod, displayMetrics );

			platform->dpiScale = (*pEnv)->GetFloatField( pEnv, displayMetrics, densityField );
		}

	}

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

bool imappPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	return true;
}

void imappPlatformShutdown( ImAppPlatform* platform )
{
	platform->allocator = NULL;
}

sint64 imappPlatformTick( ImAppPlatform* platform, sint64 lastTickValue, sint64 tickInterval )
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

	if( waitDuration > 0 )
	{
		// TODO
	}

	return currentTickValue;
}

//////////////////////////////////////////////////////////////////////////
// Window

struct ImAppWindow
{
	ImUiAllocator*		allocator;
	ImAppPlatform*		platform;
    ImAppWindowDoUiFunc uiFunc;

	ImAppEventQueue		eventQueue;

	ANativeWindow*		pNativeWindow;
	bool				isOpen;

	EGLDisplay			display;
	EGLSurface			surface;
	EGLContext			context;
	bool				hasDeviceLost;
	bool				hasDeviceChange;

	int					width;
	int					height;
};

static void	ImAppPlatformWindowHandleWindowChangedEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent );
static void	ImAppPlatformWindowHandleWindowResizeEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent );
static void	ImAppPlatformWindowHandleInputChangedEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent );

static bool	ImAppPlatformWindowHandleInputEvent( ImAppWindow* window, const AInputEvent* pInputEvent );

ImAppWindow* imappPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowStyle style, ImAppWindowState state, ImAppWindowDoUiFunc uiFunc )
{
	ImAppWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppWindow );
	if( window == NULL )
	{
		return NULL;
	}

	window->allocator	= platform->allocator;
	window->platform	= platform;
    window->uiFunc      = uiFunc;
	window->isOpen		= true;
	window->display		= EGL_NO_DISPLAY;
	window->surface		= EGL_NO_SURFACE;
	window->context		= EGL_NO_CONTEXT;

	imappEventQueueConstruct( &window->eventQueue, platform->allocator );

	while( window->isOpen &&
		   window->pNativeWindow == NULL )
	{
		imappPlatformWindowUpdate( window, NULL, NULL );
	}

	if( window->pNativeWindow == NULL )
	{
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	window->hasDeviceChange = false;

	return window;
}

void imappPlatformWindowDestroy( ImAppWindow* window )
{
	IMAPP_ASSERT( window->context == EGL_NO_CONTEXT );

	imappEventQueueDestruct( &window->eventQueue );

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
		EGL_CONTEXT_CLIENT_VERSION,	3,
		EGL_NONE
	};

	window->context = eglCreateContext( window->display, config, NULL, contextAttributes );

	if( eglMakeCurrent( window->display, window->surface, window->surface, window->context ) == EGL_FALSE )
	{
		return false;
	}

	eglQuerySurface( window->display, window->surface, EGL_WIDTH, &window->width );
	eglQuerySurface( window->display, window->surface, EGL_HEIGHT, &window->height );

	ImAppAndroidUpdateViewBoundsAndDpiScale( window->platform );

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

ImAppWindowDeviceState imappPlatformWindowGetGlContextState( const ImAppWindow* window )
{
	if( window->display == EGL_NO_DISPLAY )
	{
		return ImAppWindowDeviceState_NoDevice;
	}
	else if( window->hasDeviceLost)
	{
		return ImAppWindowDeviceState_DeviceLost;
	}
	else if( window->hasDeviceChange )
	{
		return ImAppWindowDeviceState_NewDevice;
	}

	return ImAppWindowDeviceState_Ok;
}

void imappPlatformWindowUpdate( ImAppWindow* window, ImAppPlatformWindowUpdateCallback callback, void* arg )
{
	int eventCount;
	int eventType;
	void* pUserData;
	while( (eventType = ALooper_pollOnce( 0, NULL, &eventCount, &pUserData )) >= 0 )
	{
		IMAPP_ASSERT( pUserData == window->platform );

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

				const bool handled = ImAppPlatformWindowHandleInputEvent( window, pInputEvent );
				AInputQueue_finishEvent( window->platform->pInputQueue, pInputEvent, handled );
			}
		}
	}
}

static void ImAppPlatformWindowHandleWindowChangedEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent )
{
	window->pNativeWindow = pSystemEvent->data.window.window;

	if( !window->pNativeWindow )
	{
		window->hasDeviceLost = true;
		window->isOpen = false;
	}
	else if( window->hasDeviceLost )
	{
		window->isOpen = ImAppPlatformWindowCreateGlContext( window );
		window->hasDeviceLost = false;
		window->hasDeviceChange = true;
	}
}

static void	ImAppPlatformWindowHandleWindowResizeEvent( ImAppWindow* window, const ImAppAndroidEvent* pSystemEvent )
{
	IMAPP_ASSERT( window->pNativeWindow == pSystemEvent->data.window.window );

	window->width	= ANativeWindow_getWidth( window->pNativeWindow );
	window->height	= ANativeWindow_getHeight( window->pNativeWindow );

	ImAppAndroidUpdateViewBoundsAndDpiScale( window->platform );
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
	const int32_t eventType = AInputEvent_getType( pInputEvent );
	switch( eventType )
	{
	case AINPUT_EVENT_TYPE_KEY:
		{
			// not implemented
			const int32_t keyAction = AKeyEvent_getAction( pInputEvent );
			switch( keyAction )
			{
			case AKEY_EVENT_ACTION_DOWN:
			case AKEY_EVENT_ACTION_UP:
				{
                    const int32_t keyCode = AKeyEvent_getKeyCode( pInputEvent );

                    // Text Input
                    {
                        JNIEnv* jniEnv = NULL;
                        (*window->platform->pActivity->vm)->AttachCurrentThread( window->platform->pActivity->vm, &jniEnv, NULL );

                        jclass keyEventClass = (*jniEnv)->FindClass( jniEnv, "android/view/KeyEvent" );
                        jmethodID keyEventConstructor = (*jniEnv)->GetMethodID( jniEnv, keyEventClass, "<init>", "(II)V");
                        jobject keyEventObj = (*jniEnv)->NewObject( jniEnv, keyEventClass, keyEventConstructor, eventType, keyCode );

                        int unicodeKey;
                        const int32_t metaState = AKeyEvent_getMetaState( pInputEvent );
                        if( metaState == 0 )
                        {
                            jmethodID getUnicodeCharMethod = (*jniEnv)->GetMethodID( jniEnv, keyEventClass, "getUnicodeChar", "()I");
                            unicodeKey = (*jniEnv)->CallIntMethod( jniEnv, keyEventObj, getUnicodeCharMethod );
                        }
                        else
                        {
                            jmethodID getUnicodeCharMethod = (*jniEnv)->GetMethodID( jniEnv, keyEventClass, "getUnicodeChar", "(I)I");
                            unicodeKey = (*jniEnv)->CallIntMethod( jniEnv, keyEventObj, getUnicodeCharMethod, metaState );
                        }

                        (*window->platform->pActivity->vm)->DetachCurrentThread( window->platform->pActivity->vm );

                        if( unicodeKey != 0 )
                        {
                            const ImAppEvent charEvent = { .character = { .type = ImAppEventType_Character, .character = unicodeKey } };
                            imappEventQueuePush( &window->eventQueue, &charEvent );
                        }
                    }

                    // Key Press
                    {
                        if( keyCode > IMAPP_ARRAY_COUNT( window->platform->inputKeyMapping ) )
                        {
                            break;
                        }

                        const ImUiInputKey key = (ImUiInputKey)window->platform->inputKeyMapping[ keyCode ];
                        if( key == ImUiInputKey_None )
                        {
                            break;
                        }

                        const uint32_t repeatCount = AKeyEvent_getRepeatCount( pInputEvent );
                        bool repeat = false;
                        if( repeatCount != window->platform->inputKeyRepeat[ key ] )
                        {
                            window->platform->inputKeyRepeat[ key ] = repeatCount;
                            repeat = true;
                        }

                        const ImAppEventType eventType = keyAction == AKEY_EVENT_ACTION_DOWN ? ImAppEventType_KeyDown : ImAppEventType_KeyUp;
                        const ImAppEvent keyEvent = { .key = { .type = eventType, .key = key, .repeat = repeat } };
                        imappEventQueuePush( &window->eventQueue, &keyEvent );
                    }
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
					imappEventQueuePush( &window->eventQueue, &motionEvent );

					if( motionAction != AMOTION_EVENT_ACTION_MOVE )
					{
						const ImAppEventType eventType	= motionAction == AMOTION_EVENT_ACTION_DOWN ? ImAppEventType_ButtonDown : ImAppEventType_ButtonUp;
						const ImAppEvent buttonEvent	= { .button = { .type = eventType, .x = x, .y = y, .button = ImUiInputMouseButton_Left, .repeateCount = 1 } };
						imappEventQueuePush( &window->eventQueue, &buttonEvent );
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

void imappPlatformWindowBeginRender( ImAppWindow* window )
{
	if( window->display == EGL_NO_DISPLAY )
	{
		return false;
	}

	if( eglMakeCurrent( window->display, window->surface, window->surface, window->context ) == EGL_FALSE )
	{
		return false;
	}

	return true;
}

bool imappPlatformWindowEndRender( ImAppWindow* window )
{
	if( window->display == EGL_NO_DISPLAY )
	{
		return false;
	}

	if( window->hasDeviceLost )
	{
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}

	window->hasDeviceChange = false;

	eglSwapBuffers( window->display, window->surface );
	return true;
}

ImAppEventQueue* imappPlatformWindowGetEventQueue( ImAppWindow* window )
{
	return &window->eventQueue;
}

ImAppWindowDoUiFunc ImAppPlatformWindowGetUiFunc( ImAppWindow* window )
{
    return  window->uiFunc;
}


bool imappPlatformWindowPopDropData( ImAppWindow* window, ImAppDropData* outData )
{
	return false;
}

void imappPlatformWindowGetViewRect( const ImAppWindow* window, int* pX, int* pY, int* pWidth, int* pHeight )
{
	*pX			= 0u; //window->platform->viewLeft;
	*pY			= window->platform->viewTop;
	*pWidth		= window->platform->viewWidth;
	*pHeight	= window->platform->viewHeight;
}

bool imappPlatformWindowHasFocus( const ImAppWindow* window )
{
    return true;
}

void imappPlatformWindowGetSize( const ImAppWindow* window, int* pWidth, int* pHeight )
{
	*pWidth		= window->width;
	*pHeight	= window->height;
}

void imappPlatformWindowSetSize( const ImAppWindow* window, int width, int height )
{
    // not supported
}

void imappPlatformWindowGetPosition( const ImAppWindow* window, int* pX, int* pY )
{
	*pX			= 0u;
	*pY			= window->platform->viewTop;
}

void imappPlatformWindowSetPosition( const ImAppWindow* window, int x, int y )
{
    // not supported
}

ImAppWindowState imappPlatformWindowGetState( const ImAppWindow* window )
{
	return ImAppWindowState_Maximized;
}

void imappPlatformWindowSetState( ImAppWindow* window, ImAppWindowState state )
{
    // not supported
}

const char* imappPlatformWindowGetTitle( const ImAppWindow* window )
{
    return NULL;
}

void imappPlatformWindowSetTitle( ImAppWindow* window, const char* title )
{
    // not supported
}

void imappPlatformWindowSetTitleBounds( ImAppWindow* window, int height, int buttonsX )
{
    // not supported
}

float imappPlatformWindowGetDpiScale( const ImAppWindow* window )
{
	return window->platform->dpiScale;
}

void imappPlatformWindowClose( ImAppWindow* window )
{
    // not supported
}

//////////////////////////////////////////////////////////////////////////
// Resources

void imappPlatformResourceGetPath( ImAppPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName )
{
	strncpy( outPath, resourceName, pathCapacity );
}

ImAppBlob imappPlatformResourceLoad( ImAppPlatform* platform, const char* resourceName )
{
	AAsset* asset = AAssetManager_open( platform->pActivity->assetManager, resourceName, AASSET_MODE_BUFFER );
	if( asset == NULL )
	{
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	const uintsize size = AAsset_getLength64( asset );
	void* data = ImUiMemoryAlloc( platform->allocator, size );

	const void* sourceData = AAsset_getBuffer( asset );
	memcpy( data, sourceData, size );

	AAsset_close( asset );

	const ImAppBlob result = { data, size };
	return result;
}

ImAppBlob imappPlatformResourceLoadRange( ImAppPlatform* platform, const char* resourceName, uintsize offset, uintsize length )
{
	ImAppBlob result = { NULL, 0u };

	ImAppFile* file = imappPlatformResourceOpen( platform, resourceName );
	if( !file )
	{
		return result;
	}

	if( length == (uintsize)-1 )
	{
		length = AAsset_getLength64( (AAsset*)file ) - offset;
	}

	void* memory = ImUiMemoryAlloc( platform->allocator, length );
	if( !memory )
	{
		imappPlatformResourceClose( platform, file );
		return result;
	}

	const uintsize readResult = imappPlatformResourceRead( file, memory, length, offset );
	imappPlatformResourceClose( platform, file );

	if( readResult != length )
	{
		ImUiMemoryFree( platform->allocator, memory );
		return result;
	}

	result.data	= memory;
	result.size	= length;
	return result;
}

ImAppFile* imappPlatformResourceOpen( ImAppPlatform* platform, const char* resourceName )
{
	AAsset* asset = AAssetManager_open( platform->pActivity->assetManager, resourceName, AASSET_MODE_BUFFER );
	if( asset == NULL )
	{
		return NULL;
	}

	return (ImAppFile*)asset;
}

uintsize imappPlatformResourceRead( ImAppFile* file, void* outData, uintsize length, uintsize offset )
{
	AAsset* asset = (AAsset*)file;

	const uintsize size = AAsset_getLength64( asset );
	if( offset >= size )
	{
		return 0u;
	}
	else if( offset + length > size )
	{
		length = size - offset;
	}

	const byte* sourceData = (const byte*)AAsset_getBuffer( asset );
	memcpy( outData, sourceData + offset, length );

	return length;
}

void imappPlatformResourceClose( ImAppPlatform* platform, ImAppFile* file )
{
	AAsset* asset = (AAsset*)file;

	AAsset_close( asset );
}

ImAppBlob imappPlatformResourceLoadSystemFont( ImAppPlatform* platform, const char* fontName )
{
	static const char* s_fontDirectories[] =
	{
		"/system/fonts",
		"/system/font",
		"/data/fonts"
	};

	char path[ 256u ];
	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( s_fontDirectories ); ++i )
	{
		snprintf( path, sizeof( path ), "%s/%s", s_fontDirectories[ i ], fontName );

		FILE* file = fopen( path, "rb" );
		if( !file )
		{
			continue;
		}

		const int fd = fileno( file );
		struct stat st;
		fstat( fd, &st );

		void* data = ImUiMemoryAlloc( platform->allocator, st.st_size );
		if( !data )
		{
			fclose( file );

			const ImAppBlob result = { NULL, 0u };
			return result;
		}

		fread( data, 1u, st.st_size, file );
		fclose( file );

		const ImAppBlob result = { data, st.st_size };
		return result;
	}

	const ImAppBlob result = { NULL, 0u };
	return result;
}

void imappPlatformResourceFree( ImAppPlatform* platform, ImAppBlob blob )
{
	ImUiMemoryFree( platform->allocator, blob.data );
}

ImAppFileWatcher* imappPlatformFileWatcherCreate( ImAppPlatform* platform )
{
	return NULL;
}

void imappPlatformFileWatcherDestroy( ImAppPlatform* platform, ImAppFileWatcher* watcher )
{
}

void imappPlatformFileWatcherAddPath( ImAppFileWatcher* watcher, const char* path )
{
}

void imappPlatformFileWatcherRemovePath( ImAppFileWatcher* watcher, const char* path )
{
}

bool imappPlatformFileWatcherPopEvent( ImAppFileWatcher* watcher, ImAppFileWatchEvent* outEvent )
{
	return false;
}

#endif

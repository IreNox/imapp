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

typedef struct ImappPlatform
{
	ANativeActivity*		nativeActivity;

	int						eventPipeRead;
	int						eventPipeWrite;

	pthread_mutex_t			threadConditionMutex;
	pthread_cond_t			threadCondition;
	pthread_t				thread;

	ALooper*				pLooper;
	AInputQueue*			pInputQueue;
	uint8					inputKeyMapping[ 161u ];
    uint32					inputKeyRepeat[ ImuiInputKey_MAX ];

	int						viewTop;
	int						viewLeft;
	int						viewWidth;
	int						viewHeight;
	float					dpiScale;

	bool					started;
	bool					stopped;

	ImuiAllocator*			allocator;
} ImappPlatform;

#include "imapp_platform_pthread.h"

typedef enum ImappAndroidLooperType
{
	ImappAndroidLooperType_Invalid,
	ImappAndroidLooperType_Event,
	ImappAndroidLooperType_Input
} ImappAndroidLooperType;

typedef enum ImappAndroidEventType
{
	ImappAndroidEventType_StateChanged,
	ImappAndroidEventType_WindowChanged,
	ImappAndroidEventType_WindowResize,
	ImappAndroidEventType_InputChanged,
	ImappAndroidEventType_Shutdown
} ImappAndroidEventType;

typedef enum ImappAndroidStateType
{
	ImappAndroidStateType_Start,
	ImappAndroidStateType_Pause,
	ImappAndroidStateType_Resume,
	ImappAndroidStateType_Stop
} ImappAndroidStateType;

typedef union ImappAndroidEventData
{
	struct
	{
		ImappAndroidStateType	state;
	} state;
	struct
	{
		ANativeWindow*			window;
	} window;
	struct
	{
		AInputQueue*			pInputQueue;
	} input;
} ImappAndroidEventData;

typedef struct ImappAndroidEvent
{
	ImappAndroidEventType		type;
	ImappAndroidEventData		data;
} ImappAndroidEvent;

static void*	imappAndroidCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize );
static void		imappAndroidDestroy( ImappPlatform* platform );

static void*	imappAndroidThreadEntryPoint( void* pArgument );

static void		imappAndroidUpdateViewBoundsAndDpiScale( ImappPlatform* platform );

static bool		imappAndroidEventPop( ImappAndroidEvent* pTarget, ImappPlatform* platform );
static void		imappAndroidEventPush( ImappPlatform* platform, const ImappAndroidEvent* pEvent );

static void		imappAndroidOnStart( ANativeActivity* pActivity );
static void		imappAndroidOnPause( ANativeActivity* pActivity );
static void		imappAndroidOnResume( ANativeActivity* pActivity );
static void		imappAndroidOnStop( ANativeActivity* pActivity );
static void		imappAndroidOnDestroy( ANativeActivity* pActivity );
static void		imappAndroidOnLowMemory( ANativeActivity* pActivity );
static void*	imappAndroidOnSaveInstanceState( ANativeActivity* pActivity, size_t* pOutLen );
static void		imappAndroidOnConfigurationChanged( ANativeActivity* pActivity );
static void		imappAndroidOnWindowFocusChanged( ANativeActivity* pActivity, int focused );
static void		imappAndroidOnNativeWindowCreated( ANativeActivity* pActivity, ANativeWindow* window );
static void		imappAndroidOnNativeWindowDestroyed( ANativeActivity* pActivity, ANativeWindow* window );
static void		imappAndroidOnNativeWindowResized( ANativeActivity* pActivity, ANativeWindow* window );
static void		imappAndroidOnInputQueueCreated( ANativeActivity* pActivity, AInputQueue* pInputQueue );
static void		imappAndroidOnInputQueueDestroyed( ANativeActivity* pActivity, AInputQueue* pInputQueue );

void imappPlatformShowError( ImappPlatform* platform, const char* message )
{
}

void imappPlatformSetMouseCursor( ImappPlatform* platform, ImuiInputMouseCursor cursor )
{
}

void imappPlatformSetClipboardText( ImappPlatform* platform, const char* text )
{
	JNIEnv* env = NULL;
	(*platform->nativeActivity->vm)->AttachCurrentThread(platform->nativeActivity->vm, &env, NULL );

	jclass activityClass = (*env)->GetObjectClass( env, platform->nativeActivity->clazz );
	jmethodID getSystemServiceMethod = (*env)->GetMethodID( env, activityClass, "getSystemService", "()Ljava/lang/Object;" );

	jobject clipboardManager = (*env)->CallObjectMethod(env, platform->nativeActivity->clazz, getSystemServiceMethod, "clipboard" );

	jclass clipboardManagerClass = (*env)->GetObjectClass( env, clipboardManager );
	jmethodID setTextMethod = (*env)->GetMethodID( env, clipboardManagerClass, "setText", "(Ljava/lang/CharSequence;)V" );

	jstring textString = (*env)->NewStringUTF( env, text );
	(*env)->CallVoidMethod( env, clipboardManager, setTextMethod, textString );

	(*env)->DeleteLocalRef( env, textString );
	(*env)->DeleteLocalRef( env, activityClass );

	(*platform->nativeActivity->vm)->DetachCurrentThread(platform->nativeActivity->vm );
}

void imappPlatformGetClipboardText( ImappPlatform* platform, ImuiContext* imui )
{
	JNIEnv* pEnv = NULL;
	(*platform->nativeActivity->vm)->AttachCurrentThread(platform->nativeActivity->vm, &pEnv, NULL );

	jclass activityClass = (*pEnv)->GetObjectClass( pEnv, platform->nativeActivity->clazz );
	jmethodID getSystemServiceMethod = (*pEnv)->GetMethodID( pEnv, activityClass, "getSystemService", "()Ljava/lang/Object;" );

	jobject clipboardManager = (*pEnv)->CallObjectMethod(pEnv, platform->nativeActivity->clazz, getSystemServiceMethod, "clipboard" );

	jclass clipboardManagerClass = (*pEnv)->GetObjectClass( pEnv, clipboardManager );
	jmethodID getTextMethod = (*pEnv)->GetMethodID( pEnv, clipboardManagerClass, "getText", "()Ljava/lang/CharSequence;" );

	jstring textString = (*pEnv)->CallObjectMethod( pEnv, clipboardManager, getTextMethod );

	const char* text = (*pEnv)->GetStringUTFChars( pEnv, textString, NULL );
	imuiInputSetPasteText( imui, text );

	(*platform->nativeActivity->vm)->DetachCurrentThread(platform->nativeActivity->vm );
}

void ANativeActivity_onCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize )
{
	IMAPP_DEBUG_LOGI( "Creating: %p\n", pActivity );

	pActivity->callbacks->onStart					= imappAndroidOnStart;
	pActivity->callbacks->onPause					= imappAndroidOnPause;
	pActivity->callbacks->onResume					= imappAndroidOnResume;
	pActivity->callbacks->onStop					= imappAndroidOnStop;
	pActivity->callbacks->onDestroy					= imappAndroidOnDestroy;
	pActivity->callbacks->onLowMemory				= imappAndroidOnLowMemory;
	pActivity->callbacks->onSaveInstanceState		= imappAndroidOnSaveInstanceState;
	pActivity->callbacks->onConfigurationChanged	= imappAndroidOnConfigurationChanged;
	pActivity->callbacks->onWindowFocusChanged		= imappAndroidOnWindowFocusChanged;
	pActivity->callbacks->onNativeWindowCreated		= imappAndroidOnNativeWindowCreated;
	pActivity->callbacks->onNativeWindowDestroyed	= imappAndroidOnNativeWindowDestroyed;
	pActivity->callbacks->onNativeWindowResized		= imappAndroidOnNativeWindowResized;
	pActivity->callbacks->onInputQueueCreated		= imappAndroidOnInputQueueCreated;
	pActivity->callbacks->onInputQueueDestroyed		= imappAndroidOnInputQueueDestroyed;

	pActivity->instance = imappAndroidCreate( pActivity, savedState, savedStateSize );
}

static void* imappAndroidCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize )
{
	ImappPlatform* platform = (ImappPlatform*)malloc( sizeof( *platform ) );
    memset( platform, 0, sizeof( *platform ) );

	platform->nativeActivity = pActivity;

#if IMAPP_ENABLED( IMAPP_DEBUG )
	uintsize maxScanCode = 0u;
#endif
	for( uintsize i = 0u; i < ImuiInputKey_MAX; ++i )
	{
		const ImuiInputKey keyValue = (ImuiInputKey)i;

		uint16 scanCode = 0u;
		switch( keyValue )
		{
		case ImuiInputKey_None:				break;
		case ImuiInputKey_A:				scanCode = AKEYCODE_A; break;
		case ImuiInputKey_B:				scanCode = AKEYCODE_B; break;
		case ImuiInputKey_C:				scanCode = AKEYCODE_C; break;
		case ImuiInputKey_D:				scanCode = AKEYCODE_D; break;
		case ImuiInputKey_E:				scanCode = AKEYCODE_E; break;
		case ImuiInputKey_F:				scanCode = AKEYCODE_F; break;
		case ImuiInputKey_G:				scanCode = AKEYCODE_G; break;
		case ImuiInputKey_H:				scanCode = AKEYCODE_H; break;
		case ImuiInputKey_I:				scanCode = AKEYCODE_I; break;
		case ImuiInputKey_J:				scanCode = AKEYCODE_J; break;
		case ImuiInputKey_K:				scanCode = AKEYCODE_K; break;
		case ImuiInputKey_L:				scanCode = AKEYCODE_L; break;
		case ImuiInputKey_M:				scanCode = AKEYCODE_M; break;
		case ImuiInputKey_N:				scanCode = AKEYCODE_N; break;
		case ImuiInputKey_O:				scanCode = AKEYCODE_O; break;
		case ImuiInputKey_P:				scanCode = AKEYCODE_P; break;
		case ImuiInputKey_Q:				scanCode = AKEYCODE_Q; break;
		case ImuiInputKey_R:				scanCode = AKEYCODE_R; break;
		case ImuiInputKey_S:				scanCode = AKEYCODE_S; break;
		case ImuiInputKey_T:				scanCode = AKEYCODE_T; break;
		case ImuiInputKey_U:				scanCode = AKEYCODE_U; break;
		case ImuiInputKey_V:				scanCode = AKEYCODE_V; break;
		case ImuiInputKey_W:				scanCode = AKEYCODE_W; break;
		case ImuiInputKey_X:				scanCode = AKEYCODE_X; break;
		case ImuiInputKey_Y:				scanCode = AKEYCODE_Y; break;
		case ImuiInputKey_Z:				scanCode = AKEYCODE_Z; break;
		case ImuiInputKey_1:				scanCode = AKEYCODE_1; break;
		case ImuiInputKey_2:				scanCode = AKEYCODE_2; break;
		case ImuiInputKey_3:				scanCode = AKEYCODE_3; break;
		case ImuiInputKey_4:				scanCode = AKEYCODE_4; break;
		case ImuiInputKey_5:				scanCode = AKEYCODE_5; break;
		case ImuiInputKey_6:				scanCode = AKEYCODE_6; break;
		case ImuiInputKey_7:				scanCode = AKEYCODE_7; break;
		case ImuiInputKey_8:				scanCode = AKEYCODE_8; break;
		case ImuiInputKey_9:				scanCode = AKEYCODE_9; break;
		case ImuiInputKey_0:				scanCode = AKEYCODE_0; break;
		case ImuiInputKey_Enter:			scanCode = AKEYCODE_ENTER; break;
		case ImuiInputKey_Escape:			scanCode = AKEYCODE_ESCAPE; break;
		case ImuiInputKey_Backspace:		scanCode = AKEYCODE_DEL; break;
		case ImuiInputKey_Tab:				scanCode = AKEYCODE_TAB; break;
		case ImuiInputKey_Space:			scanCode = AKEYCODE_SPACE; break;
		case ImuiInputKey_LeftShift:		scanCode = AKEYCODE_SHIFT_LEFT; break;
		case ImuiInputKey_RightShift:		scanCode = AKEYCODE_SHIFT_RIGHT; break;
		case ImuiInputKey_LeftControl:		scanCode = AKEYCODE_CTRL_LEFT; break;
		case ImuiInputKey_RightControl:		scanCode = AKEYCODE_CTRL_RIGHT; break;
		case ImuiInputKey_LeftAlt:			scanCode = AKEYCODE_ALT_LEFT; break;
		case ImuiInputKey_RightAlt:			scanCode = AKEYCODE_ALT_RIGHT; break;
		case ImuiInputKey_Minus:			scanCode = AKEYCODE_MINUS; break;
		case ImuiInputKey_Equals:			scanCode = AKEYCODE_EQUALS; break;
		case ImuiInputKey_LeftBracket:		scanCode = AKEYCODE_LEFT_BRACKET; break;
		case ImuiInputKey_RightBracket:		scanCode = AKEYCODE_RIGHT_BRACKET; break;
		case ImuiInputKey_Backslash:		scanCode = AKEYCODE_BACKSLASH; break;
		case ImuiInputKey_Semicolon:		scanCode = AKEYCODE_SEMICOLON; break;
		case ImuiInputKey_Apostrophe:		scanCode = AKEYCODE_APOSTROPHE; break;
		case ImuiInputKey_Grave:			scanCode = AKEYCODE_GRAVE; break;
		case ImuiInputKey_Comma:			scanCode = AKEYCODE_COMMA; break;
		case ImuiInputKey_Period:			scanCode = AKEYCODE_PERIOD; break;
		case ImuiInputKey_Slash:			scanCode = AKEYCODE_SLASH; break;
		case ImuiInputKey_F1:				scanCode = AKEYCODE_F1; break;
		case ImuiInputKey_F2:				scanCode = AKEYCODE_F2; break;
		case ImuiInputKey_F3:				scanCode = AKEYCODE_F3; break;
		case ImuiInputKey_F4:				scanCode = AKEYCODE_F4; break;
		case ImuiInputKey_F5:				scanCode = AKEYCODE_F5; break;
		case ImuiInputKey_F6:				scanCode = AKEYCODE_F6; break;
		case ImuiInputKey_F7:				scanCode = AKEYCODE_F7; break;
		case ImuiInputKey_F8:				scanCode = AKEYCODE_F8; break;
		case ImuiInputKey_F9:				scanCode = AKEYCODE_F9; break;
		case ImuiInputKey_F10:				scanCode = AKEYCODE_F10; break;
		case ImuiInputKey_F11:				scanCode = AKEYCODE_F11; break;
		case ImuiInputKey_F12:				scanCode = AKEYCODE_F12; break;
		case ImuiInputKey_Print:			scanCode = AKEYCODE_SYSRQ; break;
		case ImuiInputKey_Pause:			scanCode = AKEYCODE_BREAK; break;
		case ImuiInputKey_Insert:			scanCode = AKEYCODE_INSERT; break;
		case ImuiInputKey_Delete:			scanCode = AKEYCODE_FORWARD_DEL; break;
		case ImuiInputKey_Home:				scanCode = AKEYCODE_MOVE_HOME; break;
		case ImuiInputKey_End:				scanCode = AKEYCODE_MOVE_END; break;
		case ImuiInputKey_PageUp:			scanCode = AKEYCODE_PAGE_UP; break;
		case ImuiInputKey_PageDown:			scanCode = AKEYCODE_PAGE_DOWN; break;
		case ImuiInputKey_Up:				scanCode = AKEYCODE_DPAD_UP; break;
		case ImuiInputKey_Left:				scanCode = AKEYCODE_DPAD_LEFT; break;
		case ImuiInputKey_Down:				scanCode = AKEYCODE_DPAD_DOWN; break;
		case ImuiInputKey_Right:			scanCode = AKEYCODE_DPAD_RIGHT; break;
		case ImuiInputKey_Numpad_Divide:	scanCode = AKEYCODE_NUMPAD_DIVIDE; break;
		case ImuiInputKey_Numpad_Multiply:	scanCode = AKEYCODE_NUMPAD_MULTIPLY; break;
		case ImuiInputKey_Numpad_Minus:		scanCode = AKEYCODE_NUMPAD_SUBTRACT; break;
		case ImuiInputKey_Numpad_Plus:		scanCode = AKEYCODE_NUMPAD_ADD; break;
		case ImuiInputKey_Numpad_Enter:		scanCode = AKEYCODE_NUMPAD_ENTER; break;
		case ImuiInputKey_Numpad_1:			scanCode = AKEYCODE_NUMPAD_1; break;
		case ImuiInputKey_Numpad_2:			scanCode = AKEYCODE_NUMPAD_2; break;
		case ImuiInputKey_Numpad_3:			scanCode = AKEYCODE_NUMPAD_3; break;
		case ImuiInputKey_Numpad_4:			scanCode = AKEYCODE_NUMPAD_4; break;
		case ImuiInputKey_Numpad_5:			scanCode = AKEYCODE_NUMPAD_5; break;
		case ImuiInputKey_Numpad_6:			scanCode = AKEYCODE_NUMPAD_6; break;
		case ImuiInputKey_Numpad_7:			scanCode = AKEYCODE_NUMPAD_7; break;
		case ImuiInputKey_Numpad_8:			scanCode = AKEYCODE_NUMPAD_8; break;
		case ImuiInputKey_Numpad_9:			scanCode = AKEYCODE_NUMPAD_9; break;
		case ImuiInputKey_Numpad_0:			scanCode = AKEYCODE_NUMPAD_0; break;
		case ImuiInputKey_Numpad_Period:	scanCode = AKEYCODE_NUMPAD_DOT; break;
		case ImuiInputKey_Gamepad_Dpad_Up:			scanCode = AKEYCODE_DPAD_UP; break;
		case ImuiInputKey_Gamepad_Dpad_Down:		scanCode = AKEYCODE_DPAD_DOWN; break;
		case ImuiInputKey_Gamepad_Dpad_Left:		scanCode = AKEYCODE_DPAD_LEFT; break;
		case ImuiInputKey_Gamepad_Dpad_Right:		scanCode = AKEYCODE_DPAD_RIGHT; break;
		case ImuiInputKey_Gamepad_Start:			scanCode = AKEYCODE_BUTTON_START; break;
		case ImuiInputKey_Gamepad_Back:				scanCode = AKEYCODE_BUTTON_SELECT; break;
		case ImuiInputKey_Gamepad_LeftThumb:		scanCode = AKEYCODE_BUTTON_THUMBL; break;
		case ImuiInputKey_Gamepad_RightThumb:		scanCode = AKEYCODE_BUTTON_THUMBR; break;
		case ImuiInputKey_Gamepad_LeftShoulder:		scanCode = AKEYCODE_BUTTON_L1; break;
		case ImuiInputKey_Gamepad_RightShoulder:	scanCode = AKEYCODE_BUTTON_R1; break;
		case ImuiInputKey_Gamepad_A:				scanCode = AKEYCODE_BUTTON_A; break;
		case ImuiInputKey_Gamepad_B:				scanCode = AKEYCODE_BUTTON_B; break;
		case ImuiInputKey_Gamepad_X:				scanCode = AKEYCODE_BUTTON_X; break;
		case ImuiInputKey_Gamepad_Y:				scanCode = AKEYCODE_BUTTON_Y; break;

		case ImuiInputKey_MAX:				break;
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
	pthread_create( &platform->thread, &threadAttributes, imappAndroidThreadEntryPoint, platform );

	return platform;
}

static void imappAndroidDestroy( ImappPlatform* platform )
{
	{
		ImappAndroidEvent androidEvent;
		androidEvent.type = ImappAndroidEventType_Shutdown;
		imappAndroidEventPush( platform, &androidEvent );
	}

	pthread_mutex_lock( &platform->threadConditionMutex );
	while( !platform->stopped )
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

static void* imappAndroidThreadEntryPoint( void* pArgument )
{
	ImappPlatform* platform = (ImappPlatform*)pArgument;

	platform->pLooper = ALooper_prepare( ALOOPER_PREPARE_ALLOW_NON_CALLBACKS );
	ALooper_addFd( platform->pLooper, platform->eventPipeRead, ImappAndroidLooperType_Event, ALOOPER_EVENT_INPUT, NULL, platform );

	pthread_mutex_lock( &platform->threadConditionMutex );
	platform->started = true;
	pthread_cond_broadcast( &platform->threadCondition );
	pthread_mutex_unlock( &platform->threadConditionMutex );

	imappMain( platform, 0, NULL );

	pthread_mutex_lock( &platform->threadConditionMutex );
	platform->stopped = true;
	pthread_cond_broadcast( &platform->threadCondition );
	pthread_mutex_unlock( &platform->threadConditionMutex );

	ANativeActivity_finish( platform->nativeActivity );

	return NULL;
}

static void imappAndroidUpdateViewBoundsAndDpiScale( ImappPlatform* platform )
{
	JNIEnv* pEnv = NULL;
	(*platform->nativeActivity->vm)->AttachCurrentThread(platform->nativeActivity->vm, &pEnv, NULL );

	jclass activityClass = (*pEnv)->GetObjectClass( pEnv, platform->nativeActivity->clazz );

	{
		jmethodID getWindowMethod = (*pEnv)->GetMethodID( pEnv, activityClass, "getWindow", "()Landroid/view/Window;" );

		jobject window = (*pEnv)->CallObjectMethod(pEnv, platform->nativeActivity->clazz, getWindowMethod );

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

		jobject windowManager = (*pEnv)->CallObjectMethod(pEnv, platform->nativeActivity->clazz, getWindowManagerMethod );

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

			jobject display = (*pEnv)->CallObjectMethod(pEnv, platform->nativeActivity->clazz, getDisplayMethod );

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

	(*platform->nativeActivity->vm)->DetachCurrentThread(platform->nativeActivity->vm );
}

static bool imappAndroidEventPop( ImappAndroidEvent* pTarget, ImappPlatform* platform )
{
	const int bytesRead = read( platform->eventPipeRead, pTarget, sizeof( *pTarget ) );
	IMAPP_ASSERT( bytesRead == 0 || bytesRead == sizeof( *pTarget ) );
	return bytesRead == sizeof( *pTarget );
}

static void imappAndroidEventPush( ImappPlatform* platform, const ImappAndroidEvent* pEvent )
{
	if( write( platform->eventPipeWrite, pEvent, sizeof( *pEvent ) ) != sizeof( *pEvent ) )
	{
		imappTrace( "Write to pipe failed. Error: %s\n", strerror( errno ) );
	}
}

static void imappAndroidOnStart( ANativeActivity* pActivity )
{
	imappTrace( "Start: %p\n", pActivity );

	const ImappAndroidEvent androidEvent = { ImappAndroidEventType_StateChanged, .data.state.state = ImappAndroidStateType_Start };
	imappAndroidEventPush( (ImappPlatform*)pActivity->instance, &androidEvent );
}

static void imappAndroidOnPause( ANativeActivity* pActivity )
{
	imappTrace( "Pause: %p\n", pActivity );

	const ImappAndroidEvent androidEvent = { ImappAndroidEventType_StateChanged, .data.state.state = ImappAndroidStateType_Pause };
	imappAndroidEventPush( (ImappPlatform*)pActivity->instance, &androidEvent );
}

static void imappAndroidOnResume( ANativeActivity* pActivity )
{
	imappTrace( "Resume: %p\n", pActivity );

	const ImappAndroidEvent androidEvent = { ImappAndroidEventType_StateChanged, .data.state.state = ImappAndroidStateType_Resume };
	imappAndroidEventPush( (ImappPlatform*)pActivity->instance, &androidEvent );
}

static void imappAndroidOnStop( ANativeActivity* pActivity )
{
	imappTrace( "Stop: %p\n", pActivity );

	const ImappAndroidEvent androidEvent = { ImappAndroidEventType_StateChanged, .data.state.state = ImappAndroidStateType_Stop };
	imappAndroidEventPush( (ImappPlatform*)pActivity->instance, &androidEvent );
}

static void imappAndroidOnDestroy( ANativeActivity* pActivity )
{
	imappTrace( "Destroy: %p\n", pActivity );

	ImappPlatform* platform = (ImappPlatform*)pActivity->instance;
	imappAndroidDestroy( platform );
}

static void imappAndroidOnLowMemory( ANativeActivity* pActivity )
{
	imappTrace( "LowMemory: %p\n", pActivity );
	//android_app_write_cmd( (struct android_app*)nativeActivity->instance, APP_CMD_LOW_MEMORY );
}

static void* imappAndroidOnSaveInstanceState( ANativeActivity* pActivity, size_t* pOutLen )
{
	imappTrace( "SaveInstanceState: %p\n", pActivity );
	*pOutLen = 0u;
	return NULL;
}

static void imappAndroidOnConfigurationChanged( ANativeActivity* pActivity )
{
	imappTrace( "ConfigurationChanged: %p\n", pActivity );
	//android_app_write_cmd( (struct android_app*)nativeActivity->instance, APP_CMD_CONFIG_CHANGED );
}

static void imappAndroidOnWindowFocusChanged( ANativeActivity* pActivity, int focused )
{
	imappTrace( "WindowFocusChanged: %p -- %d\n", pActivity, focused );
	//android_app_write_cmd( (struct android_app*)nativeActivity->instance, focused ? APP_CMD_GAINED_FOCUS : APP_CMD_LOST_FOCUS );
}

static void imappAndroidOnNativeWindowCreated( ANativeActivity* pActivity, ANativeWindow* window )
{
	imappTrace( "NativeWindowCreated: %p -- %p\n", pActivity, window );

	const ImappAndroidEvent androidEvent = { ImappAndroidEventType_WindowChanged, .data.window.window = window };
	imappAndroidEventPush( (ImappPlatform*)pActivity->instance, &androidEvent );
}

static void imappAndroidOnNativeWindowDestroyed( ANativeActivity* pActivity, ANativeWindow* window )
{
	imappTrace( "NativeWindowDestroyed: %p -- %p\n", pActivity, window );

	const ImappAndroidEvent androidEvent = { ImappAndroidEventType_WindowChanged, .data.window.window = NULL };
	imappAndroidEventPush( (ImappPlatform*)pActivity->instance, &androidEvent );
}

static void imappAndroidOnNativeWindowResized( ANativeActivity* pActivity, ANativeWindow* window )
{
	imappTrace( "NativeWindowResized: %p -- %p\n", pActivity, window );

	const ImappAndroidEvent androidEvent = { ImappAndroidEventType_WindowResize, .data.window.window = window };
	imappAndroidEventPush( (ImappPlatform*)pActivity->instance, &androidEvent );
}

static void imappAndroidOnInputQueueCreated( ANativeActivity* pActivity, AInputQueue* pInputQueue )
{
	imappTrace( "InputQueueCreated: %p -- %p\n", pActivity, pInputQueue );

	const ImappAndroidEvent androidEvent = { ImappAndroidEventType_InputChanged, .data.input.pInputQueue = pInputQueue };
	imappAndroidEventPush( (ImappPlatform*)pActivity->instance, &androidEvent );
}

static void imappAndroidOnInputQueueDestroyed( ANativeActivity* pActivity, AInputQueue* pInputQueue )
{
	imappTrace( "InputQueueDestroyed: %p -- %p\n", pActivity, pInputQueue );

	const ImappAndroidEvent androidEvent = { ImappAndroidEventType_InputChanged, .data.input.pInputQueue = NULL };
	imappAndroidEventPush( (ImappPlatform*)pActivity->instance, &androidEvent );
}

//////////////////////////////////////////////////////////////////////////
// Platform

bool imappPlatformInitialize( ImappPlatform* platform, ImuiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	return true;
}

void imappPlatformShutdown( ImappPlatform* platform )
{
	platform->allocator = NULL;
}

sint64 imappPlatformTick( ImappPlatform* platform, sint64 lastTickValue, sint64 tickInterval )
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

double imappPlatformTicksToSeconds( ImappPlatform* platform, sint64 ticks )
{
	return (double)ticks / 1000.0;
}

//////////////////////////////////////////////////////////////////////////
// Window

struct ImappWindow
{
	ImuiAllocator*		allocator;
	ImappPlatform*		platform;
    ImappWindowDoUiFunc uiFunc;

	ImappEventQueue		eventQueue;

	ANativeWindow*		nativeWindow;
	bool				isOpen;

	EGLDisplay			display;
	EGLSurface			surface;
	EGLContext			context;
	bool				hasDeviceLost;
	bool				hasDeviceChange;

	int					width;
	int					height;
};

static void	imappPlatformWindowHandleWindowChangedEvent( ImappWindow* window, const ImappAndroidEvent* pSystemEvent );
static void	imappPlatformWindowHandleWindowResizeEvent( ImappWindow* window, const ImappAndroidEvent* pSystemEvent );
static void	imappPlatformWindowHandleInputChangedEvent( ImappWindow* window, const ImappAndroidEvent* pSystemEvent );

static bool	imappPlatformWindowHandleInputEvent( ImappWindow* window, const AInputEvent* pInputEvent );

ImappWindow* imappPlatformWindowCreate( ImappPlatform* platform, const ImappWindowParameters* parameters )
{
	ImappWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImappWindow );
	if( window == NULL )
	{
		return NULL;
	}

	window->allocator	= platform->allocator;
	window->platform	= platform;
	window->isOpen		= true;
	window->display		= EGL_NO_DISPLAY;
	window->surface		= EGL_NO_SURFACE;
	window->context		= EGL_NO_CONTEXT;

	imappEventQueueConstruct( &window->eventQueue, platform->allocator );

	while( window->isOpen &&
	       window->nativeWindow == NULL )
	{
		imappPlatformWindowUpdate( window, NULL, NULL );
	}

	if(window->nativeWindow == NULL )
	{
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	window->hasDeviceChange = false;

	return window;
}

void imappPlatformWindowDestroy( ImappWindow* window )
{
	IMAPP_ASSERT( window->context == EGL_NO_CONTEXT );

	imappEventQueueDestruct( &window->eventQueue );

	imuiMemoryFree( window->allocator, window );
}

bool imappPlatformWindowCreateGlContext( ImappWindow* window )
{
	IMAPP_ASSERT(window->nativeWindow != NULL );
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

	ANativeWindow_setBuffersGeometry(window->nativeWindow, 0, 0, format );

	window->surface = eglCreateWindowSurface(window->display, config, window->nativeWindow, NULL );

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

	imappAndroidUpdateViewBoundsAndDpiScale( window->platform );

	return true;
}

void imappPlatformWindowDestroyGlContext( ImappWindow* window )
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

	window->nativeWindow = NULL;
}

ImappWindowDeviceState imappPlatformWindowGetGlContextState( const ImappWindow* window )
{
	if( window->display == EGL_NO_DISPLAY )
	{
		return ImappWindowDeviceState_NoDevice;
	}
	else if( window->hasDeviceLost)
	{
		return ImappWindowDeviceState_DeviceLost;
	}
	else if( window->hasDeviceChange )
	{
		return ImappWindowDeviceState_NewDevice;
	}

	return ImappWindowDeviceState_Ok;
}

void imappPlatformWindowUpdate( ImappWindow* window, imappPlatformWindowUpdateCallback callback, void* arg )
{
	int eventCount;
	int eventType;
	void* pUserData;
	while( (eventType = ALooper_pollOnce( 0, NULL, &eventCount, &pUserData )) >= 0 )
	{
		IMAPP_ASSERT( pUserData == window->platform );

		if( eventType == ImappAndroidLooperType_Event )
		{
			ImappAndroidEvent androidEvent;
			if( imappAndroidEventPop( &androidEvent, window->platform ) )
			{
				switch( androidEvent.type )
				{
				case ImappAndroidEventType_WindowChanged:
					imappPlatformWindowHandleWindowChangedEvent( window, &androidEvent );
					break;

				case ImappAndroidEventType_WindowResize:
					imappPlatformWindowHandleWindowResizeEvent( window, &androidEvent );
					break;

				case ImappAndroidEventType_InputChanged:
					imappPlatformWindowHandleInputChangedEvent( window, &androidEvent );
					break;

				default:
					break;
				}
			}
		}
		else if( eventType == ImappAndroidLooperType_Input )
		{
			AInputEvent* pInputEvent = NULL;
			while( AInputQueue_getEvent( window->platform->pInputQueue, &pInputEvent ) >= 0 )
			{
				if( AInputQueue_preDispatchEvent( window->platform->pInputQueue, pInputEvent ) )
				{
					continue;
				}

				const bool handled = imappPlatformWindowHandleInputEvent( window, pInputEvent );
				AInputQueue_finishEvent( window->platform->pInputQueue, pInputEvent, handled );
			}
		}
	}
}

static void imappPlatformWindowHandleWindowChangedEvent( ImappWindow* window, const ImappAndroidEvent* pSystemEvent )
{
	window->nativeWindow = pSystemEvent->data.window.window;

	if( !window->nativeWindow )
	{
		window->hasDeviceLost = true;
		window->isOpen = false;
	}
	else if( window->hasDeviceLost )
	{
		window->isOpen = imappPlatformWindowCreateGlContext( window );
		window->hasDeviceLost = false;
		window->hasDeviceChange = true;
	}
}

static void	imappPlatformWindowHandleWindowResizeEvent( ImappWindow* window, const ImappAndroidEvent* pSystemEvent )
{
	IMAPP_ASSERT(window->nativeWindow == pSystemEvent->data.window.window );

	window->width	= ANativeWindow_getWidth( window->nativeWindow );
	window->height	= ANativeWindow_getHeight( window->nativeWindow );

	imappAndroidUpdateViewBoundsAndDpiScale( window->platform );
}

static void imappPlatformWindowHandleInputChangedEvent( ImappWindow* window, const ImappAndroidEvent* pSystemEvent )
{
	if( window->platform->pInputQueue != NULL )
	{
		AInputQueue_detachLooper( window->platform->pInputQueue );
	}

	window->platform->pInputQueue = pSystemEvent->data.input.pInputQueue;

	if( window->platform->pInputQueue != NULL )
	{
		AInputQueue_attachLooper( window->platform->pInputQueue, window->platform->pLooper, ImappAndroidLooperType_Input, NULL, window->platform );
	}
}

static bool	imappPlatformWindowHandleInputEvent( ImappWindow* window, const AInputEvent* pInputEvent )
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
                        (*window->platform->nativeActivity->vm)->AttachCurrentThread(window->platform->nativeActivity->vm, &jniEnv, NULL );

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

                        (*window->platform->nativeActivity->vm)->DetachCurrentThread(window->platform->nativeActivity->vm );

                        if( unicodeKey != 0 )
                        {
                            const ImappEvent charEvent = { .character = { .type = ImappEventType_Character, .character = unicodeKey } };
                            imappEventQueuePush( &window->eventQueue, &charEvent );
                        }
                    }

                    // Key Press
                    {
                        if( keyCode > IMAPP_ARRAY_COUNT( window->platform->inputKeyMapping ) )
                        {
                            break;
                        }

                        const ImuiInputKey key = (ImuiInputKey)window->platform->inputKeyMapping[ keyCode ];
                        if( key == ImuiInputKey_None )
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

                        const ImappEventType eventType = keyAction == AKEY_EVENT_ACTION_DOWN ? ImappEventType_KeyDown : ImappEventType_KeyUp;
                        const ImappEvent keyEvent = { .key = { .type = eventType, .key = key, .repeat = repeat } };
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

					const ImappEvent motionEvent = { .motion = { .type = ImappEventType_Motion, .x = x, .y = y } };
					imappEventQueuePush( &window->eventQueue, &motionEvent );

					if( motionAction != AMOTION_EVENT_ACTION_MOVE )
					{
						const ImappEventType buttonEventType	= motionAction == AMOTION_EVENT_ACTION_DOWN ? ImappEventType_ButtonDown : ImappEventType_ButtonUp;
						const ImappEvent buttonEvent			= { .button = { .type = buttonEventType, .button = ImuiInputMouseButton_Left, .repeateCount = 1 } };
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

bool imappPlatformWindowBeginRender( ImappWindow* window )
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

bool imappPlatformWindowEndRender( ImappWindow* window )
{
	if( window->display == EGL_NO_DISPLAY )
	{
		return false;
	}

	if( window->hasDeviceLost )
	{
		imappPlatformWindowDestroyGlContext( window );
		return false;
	}

	window->hasDeviceChange = false;

	eglSwapBuffers( window->display, window->surface );
	return true;
}

ImappEventQueue* imappPlatformWindowGetEventQueue( ImappWindow* window )
{
	return &window->eventQueue;
}

ImappWindowDoUiFunc imappPlatformWindowGetUiFunc( ImappWindow* window )
{
    return  window->uiFunc;
}


bool imappPlatformWindowPopDropData( ImappWindow* window, ImappDropData* outData )
{
	return false;
}

void imappPlatformWindowGetViewRect( const ImappWindow* window, int* pX, int* pY, int* pWidth, int* pHeight )
{
	*pX			= 0u; //window->platform->viewLeft;
	*pY			= window->platform->viewTop;
	*pWidth		= window->platform->viewWidth;
	*pHeight	= window->platform->viewHeight;
}

bool imappPlatformWindowHasFocus( const ImappWindow* window )
{
    return true;
}

void imappPlatformWindowGetSize( const ImappWindow* window, int* pWidth, int* pHeight )
{
	*pWidth		= window->width;
	*pHeight	= window->height;
}

void imappPlatformWindowSetSize( ImappWindow* window, int width, int height )
{
    // not supported
}

void imappPlatformWindowGetPosition( const ImappWindow* window, int* pX, int* pY )
{
	*pX			= 0u;
	*pY			= window->platform->viewTop;
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
	return window->platform->dpiScale;
}

void imappPlatformWindowClose( ImappWindow* window )
{
    // not supported
}

//////////////////////////////////////////////////////////////////////////
// Resources

void imappPlatformResourceGetPath( ImappPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName )
{
	strncpy( outPath, resourceName, pathCapacity );
}

ImappBlob imappPlatformResourceLoad( ImappPlatform* platform, const char* resourceName )
{
	AAsset* asset = AAssetManager_open(platform->nativeActivity->assetManager, resourceName, AASSET_MODE_BUFFER );
	if( asset == NULL )
	{
		const ImappBlob result = { NULL, 0u };
		return result;
	}

	const uintsize size = AAsset_getLength64( asset );
	void* data = imuiMemoryAlloc( platform->allocator, size );

	const void* sourceData = AAsset_getBuffer( asset );
	memcpy( data, sourceData, size );

	AAsset_close( asset );

	const ImappBlob result = { data, size };
	return result;
}

ImappBlob imappPlatformResourceLoadRange( ImappPlatform* platform, const char* resourceName, uintsize offset, uintsize length )
{
	ImappBlob result = { NULL, 0u };

	ImappFile* file = imappPlatformResourceOpen( platform, resourceName );
	if( !file )
	{
		return result;
	}

	if( length == (uintsize)-1 )
	{
		length = AAsset_getLength64( (AAsset*)file ) - offset;
	}

	void* memory = imuiMemoryAlloc( platform->allocator, length );
	if( !memory )
	{
		imappPlatformResourceClose( platform, file );
		return result;
	}

	const uintsize readResult = imappPlatformResourceRead( file, memory, length, offset );
	imappPlatformResourceClose( platform, file );

	if( readResult != length )
	{
		imuiMemoryFree( platform->allocator, memory );
		return result;
	}

	result.data	= memory;
	result.size	= length;
	return result;
}

ImappFile* imappPlatformResourceOpen( ImappPlatform* platform, const char* resourceName )
{
	AAsset* asset = AAssetManager_open(platform->nativeActivity->assetManager, resourceName, AASSET_MODE_BUFFER );
	if( asset == NULL )
	{
		return NULL;
	}

	return (ImappFile*)asset;
}

uintsize imappPlatformResourceRead( ImappFile* file, void* outData, uintsize length, uintsize offset )
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

void imappPlatformResourceClose( ImappPlatform* platform, ImappFile* file )
{
	AAsset* asset = (AAsset*)file;

	AAsset_close( asset );
}

ImappBlob imappPlatformResourceLoadSystemFont( ImappPlatform* platform, const char* fontName )
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

		void* data = imuiMemoryAlloc( platform->allocator, st.st_size );
		if( !data )
		{
			fclose( file );

			const ImappBlob result = { NULL, 0u };
			return result;
		}

		fread( data, 1u, st.st_size, file );
		fclose( file );

		const ImappBlob result = { data, st.st_size };
		return result;
	}

	const ImappBlob result = { NULL, 0u };
	return result;
}

void imappPlatformResourceFree( ImappPlatform* platform, ImappBlob blob )
{
	imuiMemoryFree( platform->allocator, blob.data );
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

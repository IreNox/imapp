#include "imapp_android_activity.h"

#include "../imapp_defines.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )

#include <android/native_activity.h>

#include "../imapp_helper.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct ImAppAndroid
{
	ANativeActivity*		pActivity;

	int						eventPipeRead;
	int						eventPipeWrite;

	pthread_mutex_t			threadConditionMutex;
	pthread_cond_t			threadCondition;
	pthread_t				thread;

	ALooper*				pLooper;
	AInputQueue*			pInputQueue;

	ImAppWindow				window;

	bool					started;
	bool					stoped;
} ImAppAndroid;

typedef enum ImAppAndroidLooperType
{
	ImAppAndroidEventType_Event
} ImAppAndroidLooperType;

typedef enum ImAppAndroidEventType
{
	ImAppAndroidEventType_Window,
	ImAppAndroidEventType_Input,
	ImAppAndroidEventType_Shutdown
} ImAppAndroidEventType;

typedef union ImAppAndroidEventData
{
	struct
	{
		ANativeWindow*		pWindow;
	} window;
	struct
	{
		AInputQueue*		pInputQueue;
	} input;
} ImAppAndroidEventData;

typedef union ImAppAndroidEvent
{
	ImAppAndroidEventType	type;
	ImAppAndroidEventData	data;
} ImAppAndroidEvent;

static void*	ImAppAndroidCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize );
static void		ImAppAndroidDestroy( ImAppAndroid* pAndroid );
static void*	ImAppAndroidThreadEntryPoint( void* pArgument );

static bool		ImAppAnroidEventPop( ImAppAndroidEvent* pTarget, ImAppAndroid* pAndroid );
static void		ImAppAnroidEventPush( ImAppAndroid* pAndroid, const ImAppAndroidEvent* pEvent );

static void		ImAppAndroidOnStart( ANativeActivity* pActivity );
static void		ImAppAndroidOnPause( ANativeActivity* pActivity );
static void		ImAppAndroidOnResume( ANativeActivity* pActivity );
static void		ImAppAndroidOnStop( ANativeActivity* pActivity );
static void		ImAppAndroidOnDestroy( ANativeActivity* pActivity );
static void		ImAppAndroidOnLowMemory( ANativeActivity* pActivity );
static void*	ImAppAndroidOnSaveInstanceState( ANativeActivity* pActivity, size_t* outLen );
static void		ImAppAndroidOnConfigurationChanged( ANativeActivity* pActivity );
static void		ImAppAndroidOnWindowFocusChanged( ANativeActivity* pActivity, int focused );
static void		ImAppAndroidOnNativeWindowCreated( ANativeActivity* pActivity, ANativeWindow* window );
static void		ImAppAndroidOnNativeWindowDestroyed( ANativeActivity* pActivity, ANativeWindow* window );
static void		ImAppAndroidOnInputQueueCreated( ANativeActivity* pActivity, AInputQueue* queue );
static void		ImAppAndroidOnInputQueueDestroyed( ANativeActivity* pActivity, AInputQueue* queue );

void ANativeActivity_onCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize ) {
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
	ImAppAndroid* pAndroid = IMAPP_NEW_ZERO( ImAppAndroid );

	pAndroid->pActivity		= pActivity;

	pthread_mutex_init( &pAndroid->threadConditionMutex, NULL );
	pthread_cond_init( &pAndroid->threadCondition, NULL );

	int eventPipe[ 2u ];
	if( pipe( eventPipe ) )
	{
		ImAppTrace( "Could not create pipe: %s\n", strerror( errno ) );
		ImAppFree( pAndroid );
		return NULL;
	}
	pAndroid->eventPipeRead		= eventPipe[ 0u ];
	pAndroid->eventPipeWrite	= eventPipe[ 1u ];

	pthread_attr_t threadAttributes;
	pthread_attr_init( &threadAttributes );
	pthread_attr_setdetachstate( &threadAttributes, PTHREAD_CREATE_DETACHED );
	pthread_create( &pAndroid->thread, &threadAttributes, ImAppAndroidThreadEntryPoint, pAndroid );

	return pAndroid;
}

static void ImAppAndroidDestroy( ImAppAndroid* pAndroid )
{
	{
		ImAppAndroidEvent androidEvent;
		androidEvent.type = ImAppAndroidEventType_Shutdown;
		ImAppAnroidEventPush( pAndroid, &androidEvent );
	}

	pthread_mutex_lock( &pAndroid->threadConditionMutex );
	while( !pAndroid->stoped )
	{
		pthread_cond_wait( &pAndroid->threadCondition, &pAndroid->threadConditionMutex );
	}
	pthread_mutex_unlock( &pAndroid->threadConditionMutex );

	pthread_cond_destroy( &pAndroid->threadCondition );
	pthread_mutex_destroy( &pAndroid->threadConditionMutex );

	close( pAndroid->eventPipeWrite );
	close( pAndroid->eventPipeRead );

	ImAppFree( pAndroid );
}

static void* ImAppAndroidThreadEntryPoint( void* pArgument )
{
	ImAppAndroid* pAndroid = (ImAppAndroid*)pArgument;

	pAndroid->pLooper = ALooper_prepare( ALOOPER_PREPARE_ALLOW_NON_CALLBACKS );
	ALooper_addFd( pAndroid->pLooper, pAndroid->eventPipeRead, ImAppAndroidEventType_Event, ALOOPER_EVENT_INPUT, NULL, pAndroid );

	pthread_mutex_lock( &pAndroid->threadConditionMutex );
	pAndroid->started = true;
	pthread_cond_broadcast( &pAndroid->threadCondition );
	pthread_mutex_unlock( &pAndroid->threadConditionMutex );

	ImAppMain( 0, NULL );

	pthread_mutex_lock( &pAndroid->threadConditionMutex );
	pAndroid->stoped = true;
	pthread_cond_broadcast( &pAndroid->threadCondition );
	pthread_mutex_unlock( &pAndroid->threadConditionMutex );

	return NULL;
}

static bool ImAppAnroidEventPop( ImAppAndroidEvent* pTarget, ImAppAndroid* pAndroid )
{
	const int bytesRead = read( pAndroid->eventPipeRead, pTarget, sizeof( *pTarget ) );
	IMAPP_ASSERT( bytesRead == 0 || bytesRead == sizeof( *pTarget ) );
	return bytesRead == sizeof( *pTarget );
}

static void ImAppAnroidEventPush( ImAppAndroid* pAndroid, const ImAppAndroidEvent* pEvent )
{
	if( write( pAndroid->eventPipeWrite, pEvent, sizeof( *pEvent ) ) != sizeof( *pEvent ) )
	{
		ImAppTrace( "Write to pipe failed. Error: %s\n", strerror( errno ) );
	}
}

static void ImAppAndroidOnStart( ANativeActivity* pActivity )
{
	ImAppTrace( "Start: %p\n", pActivity );
	//android_app_set_activity_state( (struct android_app*)pActivity->instance, APP_CMD_START );
}

static void ImAppAndroidOnPause( ANativeActivity* pActivity )
{
	ImAppTrace( "Pause: %p\n", pActivity );
	//android_app_set_activity_state( (struct android_app*)pActivity->instance, APP_CMD_PAUSE );
}

static void ImAppAndroidOnResume( ANativeActivity* pActivity )
{
	ImAppTrace( "Resume: %p\n", pActivity );
	//android_app_set_activity_state( (struct android_app*)pActivity->instance, APP_CMD_RESUME );
}

static void ImAppAndroidOnStop( ANativeActivity* pActivity )
{
	ImAppTrace( "Stop: %p\n", pActivity );
	//android_app_set_activity_state( (struct android_app*)pActivity->instance, APP_CMD_STOP );
}

static void ImAppAndroidOnDestroy( ANativeActivity* pActivity )
{
	ImAppTrace( "Destroy: %p\n", pActivity );

	ImAppAndroid* pAndroid = (ImAppAndroid*)pActivity->instance;
	ImAppAndroidDestroy( pAndroid );
}

static void ImAppAndroidOnLowMemory( ANativeActivity* pActivity )
{
	ImAppTrace( "LowMemory: %p\n", pActivity );
	//android_app_write_cmd( (struct android_app*)pActivity->instance, APP_CMD_LOW_MEMORY );
}

static void* ImAppAndroidOnSaveInstanceState( ANativeActivity* pActivity, size_t* outLen )
{
	ImAppTrace( "SaveInstanceState: %p\n", pActivity );
	*outLen = 0u;
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
	//android_app_set_window( (struct android_app*)pActivity->instance, window );
}

static void ImAppAndroidOnNativeWindowDestroyed( ANativeActivity* pActivity, ANativeWindow* window )
{
	ImAppTrace( "NativeWindowDestroyed: %p -- %p\n", pActivity, window );
	//android_app_set_window( (struct android_app*)pActivity->instance, NULL );
}

static void ImAppAndroidOnInputQueueCreated( ANativeActivity* pActivity, AInputQueue* queue )
{
	ImAppTrace( "InputQueueCreated: %p -- %p\n", pActivity, queue );
	//android_app_set_input( (struct android_app*)pActivity->instance, queue );
}

static void ImAppAndroidOnInputQueueDestroyed( ANativeActivity* pActivity, AInputQueue* queue )
{
	ImAppTrace( "InputQueueDestroyed: %p -- %p\n", pActivity, queue );
	//android_app_set_input( (struct android_app*)pActivity->instance, NULL );
}

#endif

#include "../imapp_defines.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )

#include <android/native_activity.h>

#include "../imapp_helper.h"

#include <pthread.h>
#include <stdbool.h>

struct ImAppAndroidContext
{
	ANativeActivity*	pActivity;

	pthread_mutex_t		threadConditionMutex;
	pthread_cond_t		threadCondition;
	pthread_t			thread;

	bool				started;
	bool				stoped;
};
typedef struct ImAppAndroidContext ImAppAndroidContext;

static void*	ImAppAndroidCreate( ANativeActivity* pActivity, void* savedState, size_t savedStateSize );
static void		ImAppAndroidDestroy( ImAppAndroidContext* pContext );
static void*	ImAppAndroidThreadEntryPoint( void* pArgument );

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
	ImAppAndroidContext* pContext = IMAPP_NEW_ZERO( ImAppAndroidContext );

	pContext->pActivity		= pActivity;

	pthread_mutex_init( &pContext->threadConditionMutex, NULL );
	pthread_cond_init( &pContext->threadCondition, NULL );

	pthread_attr_t threadAttributes;
	pthread_attr_init( &threadAttributes );
	pthread_attr_setdetachstate( &threadAttributes, PTHREAD_CREATE_DETACHED );
	pthread_create( &pContext->thread, &threadAttributes, ImAppAndroidThreadEntryPoint, pContext );

	return pContext;
}

static void ImAppAndroidDestroy( ImAppAndroidContext* pContext )
{
	pthread_mutex_lock( &pContext->threadConditionMutex );
	//android_app_write_cmd( android_app, APP_CMD_DESTROY );
	while( !pContext->stoped )
	{
		pthread_cond_wait( &pContext->threadCondition, &pContext->threadConditionMutex );
	}
	pthread_mutex_unlock( &pContext->threadConditionMutex );

	pthread_cond_destroy( &pContext->threadCondition );
	pthread_mutex_destroy( &pContext->threadConditionMutex );

	ImAppFree( pContext );
}

static void* ImAppAndroidThreadEntryPoint( void* pArgument )
{
	ImAppAndroidContext* pContext = (ImAppAndroidContext*)pArgument;

	pthread_mutex_lock( &pContext->threadConditionMutex );
	pContext->started = true;
	pthread_cond_broadcast( &pContext->threadCondition );
	pthread_mutex_unlock( &pContext->threadConditionMutex );

	ImAppMain( 0, NULL );

	pthread_mutex_lock( &pContext->threadConditionMutex );
	pContext->stoped = true;
	pthread_cond_broadcast( &pContext->threadCondition );
	pthread_mutex_unlock( &pContext->threadConditionMutex );

	return NULL;
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

	ImAppAndroidContext* pContext = (ImAppAndroidContext*)pActivity->instance;
	ImAppAndroidDestroy( pContext );
}

static void ImAppAndroidOnLowMemory( ANativeActivity* pActivity )
{
	ImAppTrace( "LowMemory: %p\n", pActivity );
	//android_app_write_cmd( (struct android_app*)pActivity->instance, APP_CMD_LOW_MEMORY );
}

static void* ImAppAndroidOnSaveInstanceState( ANativeActivity* pActivity, size_t* outLen )
{
	//struct android_app* android_app = (struct android_app*)pActivity->instance;
	void* savedState = NULL;

	//ImAppTrace( "SaveInstanceState: %p\n", pActivity );
	//pthread_mutex_lock( &android_app->mutex );
	//android_app->stateSaved = 0;
	//android_app_write_cmd( android_app, APP_CMD_SAVE_STATE );
	//while( !android_app->stateSaved )
	//{
	//	pthread_cond_wait( &android_app->cond, &android_app->mutex );
	//}

	//if( android_app->savedState != NULL )
	//{
	//	savedState = android_app->savedState;
	//	*outLen = android_app->savedStateSize;
	//	android_app->savedState = NULL;
	//	android_app->savedStateSize = 0;
	//}

	//pthread_mutex_unlock( &android_app->mutex );

	return savedState;
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

#pragma once

#include "imapp_platform.h"

#include <pthread.h>
#include <semaphore.h>

struct ImAppThread
{
	ImAppPlatform*				platform;

	pthread_t					handle;

	ImAppThreadFunc				func;
	void*						arg;

	uint8						hasExit;
};

static void*	imappPlatformThreadEntry( void* voidThread );

ImAppThread* imappPlatformThreadCreate( ImAppPlatform* platform, const char* name, ImAppThreadFunc func, void* arg )
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

	const int result = pthread_create( &thread->handle, &attr, imappPlatformThreadEntry, thread );
	pthread_attr_destroy( &attr );

	if( result < 0 )
	{
		ImUiMemoryFree( platform->allocator, thread );
		return NULL;
	}

	return thread;
}

void imappPlatformThreadDestroy( ImAppThread* thread )
{
	pthread_join( thread->handle, NULL );

	ImUiMemoryFree( thread->platform->allocator, thread );
}

bool imappPlatformThreadIsRunning( const ImAppThread* thread )
{
	return __atomic_load_n( &thread->hasExit, __ATOMIC_RELAXED ) == 0;
}

static void* imappPlatformThreadEntry( void* voidThread )
{
	ImAppThread* thread = (ImAppThread*)voidThread;

	thread->func( thread->arg );

	__atomic_store_n( &thread->hasExit, 1u, __ATOMIC_RELAXED );

	return NULL;
}

ImAppMutex* imappPlatformMutexCreate( ImAppPlatform* platform )
{
	pthread_mutex_t* mutex = IMUI_MEMORY_NEW_ZERO( platform->allocator, pthread_mutex_t );
	if( pthread_mutex_init( mutex, NULL ) < 0 )
	{
		ImUiMemoryFree( platform->allocator, mutex );
		return NULL;
	}

	return (ImAppMutex*)mutex;
}

void imappPlatformMutexDestroy( ImAppPlatform* platform, ImAppMutex* mutex )
{
	pthread_mutex_destroy( (pthread_mutex_t*)mutex );
	ImUiMemoryFree( platform->allocator, mutex );
}

void imappPlatformMutexLock( ImAppMutex* mutex )
{
	pthread_mutex_lock( (pthread_mutex_t*)mutex );
}

void imappPlatformMutexUnlock( ImAppMutex* mutex )
{
	pthread_mutex_unlock( (pthread_mutex_t*)mutex );
}

ImAppSemaphore* imappPlatformSemaphoreCreate( ImAppPlatform* platform )
{
	sem_t* semaphore = IMUI_MEMORY_NEW_ZERO( platform->allocator, sem_t );
	if( sem_init( semaphore, 0, 0 ) < 0 )
	{
		ImUiMemoryFree( platform->allocator, semaphore );
		return NULL;
	}

	return (ImAppSemaphore*)semaphore;
}

void imappPlatformSemaphoreDestroy( ImAppPlatform* platform, ImAppSemaphore* semaphore )
{
	sem_destroy( (sem_t*)semaphore );
	ImUiMemoryFree( platform->allocator, semaphore );
}

void imappPlatformSemaphoreInc( ImAppSemaphore* semaphore )
{
	sem_post( (sem_t*)semaphore );
}

bool imappPlatformSemaphoreDec( ImAppSemaphore* semaphore, bool wait )
{
	if( wait )
	{
		sem_wait( (sem_t*)semaphore );
		return true;
	}

	return sem_trywait( (sem_t*)semaphore ) == 0;
}

#pragma once

#include "imapp_platform.h"

#include <pthread.h>
#include <semaphore.h>

struct ImappThread
{
	ImappPlatform*				platform;

	pthread_t					handle;

	ImappThreadFunc				func;
	void*						arg;

	uint8						hasExit;
};

static void*	imappPlatformThreadEntry( void* voidThread );

ImappThread* imappPlatformThreadCreate( ImappPlatform* platform, const char* name, ImappThreadFunc func, void* arg )
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

	ImappThread* thread = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImappThread );
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

void imappPlatformThreadDestroy( ImappThread* thread )
{
	pthread_join( thread->handle, NULL );

	ImUiMemoryFree( thread->platform->allocator, thread );
}

bool imappPlatformThreadIsRunning( const ImappThread* thread )
{
	return __atomic_load_n( &thread->hasExit, __ATOMIC_RELAXED ) == 0;
}

static void* imappPlatformThreadEntry( void* voidThread )
{
	ImappThread* thread = (ImappThread*)voidThread;

	thread->func( thread->arg );

	__atomic_store_n( &thread->hasExit, 1u, __ATOMIC_RELAXED );

	return NULL;
}

ImappMutex* imappPlatformMutexCreate( ImappPlatform* platform )
{
	pthread_mutex_t* mutex = IMUI_MEMORY_NEW_ZERO( platform->allocator, pthread_mutex_t );
	if( pthread_mutex_init( mutex, NULL ) < 0 )
	{
		ImUiMemoryFree( platform->allocator, mutex );
		return NULL;
	}

	return (ImappMutex*)mutex;
}

void imappPlatformMutexDestroy( ImappPlatform* platform, ImappMutex* mutex )
{
	pthread_mutex_destroy( (pthread_mutex_t*)mutex );
	ImUiMemoryFree( platform->allocator, mutex );
}

void imappPlatformMutexLock( ImappMutex* mutex )
{
	pthread_mutex_lock( (pthread_mutex_t*)mutex );
}

void imappPlatformMutexUnlock( ImappMutex* mutex )
{
	pthread_mutex_unlock( (pthread_mutex_t*)mutex );
}

ImappSemaphore* imappPlatformSemaphoreCreate( ImappPlatform* platform )
{
	sem_t* semaphore = IMUI_MEMORY_NEW_ZERO( platform->allocator, sem_t );
	if( sem_init( semaphore, 0, 0 ) < 0 )
	{
		ImUiMemoryFree( platform->allocator, semaphore );
		return NULL;
	}

	return (ImappSemaphore*)semaphore;
}

void imappPlatformSemaphoreDestroy( ImappPlatform* platform, ImappSemaphore* semaphore )
{
	sem_destroy( (sem_t*)semaphore );
	ImUiMemoryFree( platform->allocator, semaphore );
}

void imappPlatformSemaphoreInc( ImappSemaphore* semaphore )
{
	sem_post( (sem_t*)semaphore );
}

bool imappPlatformSemaphoreDec( ImappSemaphore* semaphore, bool wait )
{
	if( wait )
	{
		sem_wait( (sem_t*)semaphore );
		return true;
	}

	return sem_trywait( (sem_t*)semaphore ) == 0;
}

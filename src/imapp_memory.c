#include "imapp_memory.h"

#include "imapp_defines.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
#	include <windows.h>
#	include <debugapi.h>
#elif IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
#	include <android/log.h>
#endif

void*	ImAppAllocatorDefaultMalloc( size_t size, void* pUserData );
void	ImAppAllocatorDefaultFree( void* pMemory, void* pUserData );

static ImAppAllocator s_defaultAllocator =
{
	ImAppAllocatorDefaultMalloc,
	ImAppAllocatorDefaultFree,
	NULL
};

void* ImAppMalloc( ImAppAllocator* pAllocator, size_t size )
{
	return pAllocator->mallocFunc( size, pAllocator->userData );
}

void* ImAppMallocZero( ImAppAllocator* pAllocator, size_t size )
{
	void* pMemory = ImAppMalloc( pAllocator, size );
	ImAppMemoryZero( pMemory, size );
	return pMemory;
}

void ImAppFree( ImAppAllocator* pAllocator, void* pMemory )
{
	pAllocator->freeFunc( pMemory, pAllocator->userData );
}

void ImAppMemoryZero( void* pMemory, size_t size )
{
	memset( pMemory, 0, size );
}

ImAppAllocator* ImAppAllocatorGetDefault()
{
	return &s_defaultAllocator;
}

void* ImAppAllocatorDefaultMalloc( size_t size, void* pUserData )
{
	return malloc( size );
}

void ImAppAllocatorDefaultFree( void* pMemory, void* pUserData )
{
	free( pMemory );
}

void ImAppTrace( const char* pFormat, ... )
{
	char buffer[ 2048u ];

	va_list args;
	va_start( args, pFormat );
#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	vsprintf_s( buffer, 2048u, pFormat, args );
#else
	vsprintf( buffer, pFormat, args );
#endif
	va_end( args );

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	OutputDebugStringA( buffer );
#elif IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
	__android_log_write( ANDROID_LOG_INFO, "ImApp", buffer );
#else
#	error Platform not supported
#endif
}
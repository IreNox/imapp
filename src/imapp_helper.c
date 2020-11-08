#include "imapp_helper.h"

#include "imapp_platform.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
#	define _CRT_SECURE_NO_WARNINGS

#	include <windows.h>
#	include <debugapi.h>
#endif

void* ImAppMalloc( size_t size )
{
	return malloc( size );
}

void* ImAppMallocZero( size_t size )
{
	void* pMemory = ImAppMalloc( size );
	memset( pMemory, 0, size );
	return pMemory;
}

void ImAppFree( void* pMemory )
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
#else
#	error Platform not supported
#endif
}
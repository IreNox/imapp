#include "imapp_debug.h"

#include "imapp_internal.h"

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
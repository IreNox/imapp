#pragma once

#include "imapp_types.h"

#include <stdlib.h>
#include <string.h>

#define IMAPP_NEW( type ) (type*)malloc( sizeof( type ) )
#define IMAPP_NEW_ZERO( type ) (type*)ImAppMallocZero( sizeof( type ) )

inline void* ImAppMallocZero( size_t size )
{
	void* pMemory = malloc( size );
	memset( pMemory, 0, size );
	return pMemory;
}
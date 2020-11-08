#pragma once

#include <stdint.h>

#define IMAPP_NEW( type ) (type*)malloc( sizeof( type ) )
#define IMAPP_NEW_ZERO( type ) (type*)ImAppMallocZero( sizeof( type ) )

void*	ImAppMalloc( size_t size );
void*	ImAppMallocZero( size_t size );
void	ImAppFree( void* pMemory );

//#define IMAPP_NEW( allocator, type ) (type*)malloc( sizeof( type ) )
//#define IMAPP_NEW_ZERO( allocator, type ) (type*)ImAppMallocZero( sizeof( type ) )
//
//void*	ImAppMalloc( ImAppAllocator* pAllocator, size_t size );
//void*	ImAppMallocZero( ImAppAllocator* pAllocator, size_t size );
//void	ImAppFree( ImAppAllocator* pAllocator, void* pMemory );


void	ImAppTrace( const char* pFormat, ... );

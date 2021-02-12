#pragma once

#include <stdint.h>

#define IMAPP_NEW( allocator, type ) (type*)ImAppMalloc( allocator, sizeof( type ) )
#define IMAPP_NEW_ZERO( allocator, type ) (type*)ImAppMallocZero( allocator, sizeof( type ) )

void*			ImAppMalloc( ImAppAllocator* pAllocator, size_t size );
void*			ImAppMallocZero( ImAppAllocator* pAllocator, size_t size );
void			ImAppFree( ImAppAllocator* pAllocator, void* pMemory );

ImAppAllocator*	ImAppAllocatorGetDefault();

void			ImAppTrace( const char* pFormat, ... );

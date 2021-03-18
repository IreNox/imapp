#pragma once

#include <stdint.h>

#define IMAPP_NEW( allocator, type ) (type*)ImAppMalloc( allocator, sizeof( type ) )
#define IMAPP_NEW_ZERO( allocator, type ) (type*)ImAppMallocZero( allocator, sizeof( type ) )

#define IMAPP_ZERO( value ) ImAppMemoryZero( &value, sizeof( value ) )

void*				ImAppMalloc( ImAppAllocator* pAllocator, size_t size );
void*				ImAppMallocZero( ImAppAllocator* pAllocator, size_t size );
void				ImAppFree( ImAppAllocator* pAllocator, void* pMemory );

void				ImAppMemoryZero( void* pMemory, size_t size );

ImAppAllocator*		ImAppAllocatorGetDefault();

struct nk_allocator	ImAppAllocatorGetNuklear( ImAppAllocator* pAllocator );

void				ImAppTrace( const char* pFormat, ... );

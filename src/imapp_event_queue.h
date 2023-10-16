#pragma once

#include "imapp/imapp.h"

#include "imapp_event.h"

typedef struct ImAppEventQueueChunk ImAppEventQueueChunk;

typedef struct ImAppEventQueue
{
	ImUiAllocator*			allocator;

	ImAppEventQueueChunk*	currentChunk;
} ImAppEventQueue;

void	ImAppEventQueueConstruct( ImAppEventQueue* queue, ImUiAllocator* allocator );
void	ImAppEventQueueDestruct( ImAppEventQueue* queue );

void	ImAppEventQueuePush( ImAppEventQueue* queue, const ImAppEvent* event2 );
bool	ImAppEventQueuePop( ImAppEventQueue* queue, ImAppEvent* outEvent );

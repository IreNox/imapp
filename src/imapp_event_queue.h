#pragma once

#include "imapp/imapp.h"

#include "imapp_event.h"

typedef struct ImAppEventQueueChunk ImAppEventQueueChunk;

typedef struct ImAppEventQueue
{
	ImUiAllocator*			allocator;

	ImAppEventQueueChunk*	currentChunk;
} ImAppEventQueue;

void	imappEventQueueConstruct( ImAppEventQueue* queue, ImUiAllocator* allocator );
void	imappEventQueueDestruct( ImAppEventQueue* queue );

void	imappEventQueuePush( ImAppEventQueue* queue, const ImAppEvent* event2 );
bool	imappEventQueuePop( ImAppEventQueue* queue, ImAppEvent* outEvent );

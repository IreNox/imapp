#pragma once

#include "imapp/imapp.h"

#include "imapp_event.h"

typedef struct ImappEventQueueChunk ImappEventQueueChunk;

typedef struct ImappEventQueue
{
	ImuiAllocator*			allocator;

	ImappEventQueueChunk*	currentChunk;
} ImappEventQueue;

void	imappEventQueueConstruct( ImappEventQueue* queue, ImuiAllocator* allocator );
void	imappEventQueueDestruct( ImappEventQueue* queue );

void	imappEventQueuePush( ImappEventQueue* queue, const ImappEvent* event2 );
bool	imappEventQueuePop( ImappEventQueue* queue, ImappEvent* outEvent );

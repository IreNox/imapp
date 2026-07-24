#include "imapp_event_queue.h"

#include "imapp_internal.h"

#define IMAPP_EVENT_QUEUE_CHUNK_SIZE 32

typedef struct ImappEventQueueChunk ImappEventQueueChunk;
struct ImappEventQueueChunk
{
	ImappEventQueueChunk*	nextChunk;

	ImappEvent				data[ IMAPP_EVENT_QUEUE_CHUNK_SIZE ];
	size_t					top;
	size_t					bottom;
	size_t					length;
};

void imappEventQueueConstruct( ImappEventQueue* queue, ImuiAllocator* allocator )
{
	queue->allocator	= allocator;
	queue->currentChunk	= NULL;
}

void imappEventQueueDestruct( ImappEventQueue* queue )
{
	ImappEventQueueChunk* chunk = queue->currentChunk;
	while( chunk != NULL )
	{
		ImappEventQueueChunk* nextChunk = chunk->nextChunk;
		imuiMemoryFree( queue->allocator, chunk );
		chunk = nextChunk;
	}
}

void imappEventQueuePush( ImappEventQueue* queue, const ImappEvent* event2 )
{
	if( queue->currentChunk == NULL ||
		queue->currentChunk->length == IMAPP_EVENT_QUEUE_CHUNK_SIZE )
	{
		ImappEventQueueChunk* newChunk = IMUI_MEMORY_NEW_ZERO( queue->allocator, ImappEventQueueChunk );

		newChunk->nextChunk = queue->currentChunk;
		queue->currentChunk = newChunk;
	}

	ImappEventQueueChunk* chunk = queue->currentChunk;

	chunk->data[ chunk->bottom ] = *event2;
	chunk->bottom = (chunk->bottom + 1u) & (IMAPP_EVENT_QUEUE_CHUNK_SIZE - 1u);
	chunk->length++;
}

bool imappEventQueuePop( ImappEventQueue* queue, ImappEvent* outEvent )
{
	if( queue->currentChunk == NULL ||
		queue->currentChunk->length == 0u )
	{
		return false;
	}

	ImappEventQueueChunk* chunk = queue->currentChunk;

	*outEvent = chunk->data[ chunk->top ];

	chunk->top = (chunk->top + 1u) & (IMAPP_EVENT_QUEUE_CHUNK_SIZE - 1u);
	chunk->length--;

	if( chunk->length == 0u &&
		chunk->nextChunk != NULL )
	{
		queue->currentChunk = chunk->nextChunk;
		imuiMemoryFree( queue->allocator, chunk );
	}

	return true;
}

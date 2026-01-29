#include "imapp_event_queue.h"

#include "imapp_internal.h"

#define IMAPP_EVENT_QUEUE_CHUNK_SIZE 32

typedef struct ImAppEventQueueChunk ImAppEventQueueChunk;
struct ImAppEventQueueChunk
{
	ImAppEventQueueChunk*	nextChunk;

	ImAppEvent				data[ IMAPP_EVENT_QUEUE_CHUNK_SIZE ];
	size_t					top;
	size_t					bottom;
	size_t					length;
};

void imappEventQueueConstruct( ImAppEventQueue* queue, ImUiAllocator* allocator )
{
	queue->allocator	= allocator;
	queue->currentChunk	= NULL;
}

void imappEventQueueDestruct( ImAppEventQueue* queue )
{
	ImAppEventQueueChunk* chunk = queue->currentChunk;
	while( chunk != NULL )
	{
		ImAppEventQueueChunk* nextChunk = chunk->nextChunk;
		ImUiMemoryFree( queue->allocator, chunk );
		chunk = nextChunk;
	}
}

void imappEventQueuePush( ImAppEventQueue* queue, const ImAppEvent* event2 )
{
	if( queue->currentChunk == NULL ||
		queue->currentChunk->length == IMAPP_EVENT_QUEUE_CHUNK_SIZE )
	{
		ImAppEventQueueChunk* newChunk = IMUI_MEMORY_NEW_ZERO( queue->allocator, ImAppEventQueueChunk );

		newChunk->nextChunk = queue->currentChunk;
		queue->currentChunk = newChunk;
	}

	ImAppEventQueueChunk* chunk = queue->currentChunk;

	chunk->data[ chunk->bottom ] = *event2;
	chunk->bottom = (chunk->bottom + 1u) & (IMAPP_EVENT_QUEUE_CHUNK_SIZE - 1u);
	chunk->length++;
}

bool imappEventQueuePop( ImAppEventQueue* queue, ImAppEvent* outEvent )
{
	if( queue->currentChunk == NULL ||
		queue->currentChunk->length == 0u )
	{
		return false;
	}

	ImAppEventQueueChunk* chunk = queue->currentChunk;

	*outEvent = chunk->data[ chunk->top ];

	chunk->top = (chunk->top + 1u) & (IMAPP_EVENT_QUEUE_CHUNK_SIZE - 1u);
	chunk->length--;

	if( chunk->length == 0u &&
		chunk->nextChunk != NULL )
	{
		queue->currentChunk = chunk->nextChunk;
		ImUiMemoryFree( queue->allocator, chunk );
	}

	return true;
}

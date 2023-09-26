#include "imapp_event_queue.h"

#include "imapp_internal.h"

#define IMAPP_EVENT_QUEUE_CHUNK_SIZE 32

typedef struct ImAppEventQueueChunk ImAppEventQueueChunk;
struct ImAppEventQueueChunk
{
	ImAppEventQueueChunk*	pNext;

	ImAppEvent				data[ IMAPP_EVENT_QUEUE_CHUNK_SIZE ];
	size_t					top;
	size_t					bottom;
	size_t					length;
};

struct ImAppEventQueue
{
	ImUiAllocator*			pAllocator;

	ImAppEventQueueChunk*	pCurrentChunk;
};

ImAppEventQueue* ImAppEventQueueCreate( ImUiAllocator* pAllocator )
{
	ImAppEventQueue* pQueue = IMUI_MEMORY_NEW_ZERO( pAllocator, ImAppEventQueue );
	pQueue->pAllocator = pAllocator;

	return pQueue;
}

void ImAppEventQueueDestroy( ImAppEventQueue* pQueue )
{
	ImAppEventQueueChunk* pChunk = pQueue->pCurrentChunk;
	while( pChunk != NULL )
	{
		ImAppEventQueueChunk* pNextChunk = pChunk->pNext;
		ImUiMemoryFree( pQueue->pAllocator, pChunk );
		pChunk = pNextChunk;
	}

	ImUiMemoryFree( pQueue->pAllocator, pQueue );
}

void ImAppEventQueuePush( ImAppEventQueue* pQueue, const ImAppEvent* pEvent )
{
	if( pQueue->pCurrentChunk == NULL ||
		pQueue->pCurrentChunk->length == IMAPP_EVENT_QUEUE_CHUNK_SIZE )
	{
		ImAppEventQueueChunk* pNewChunk = IMUI_MEMORY_NEW_ZERO( pQueue->pAllocator, ImAppEventQueueChunk );

		pNewChunk->pNext = pQueue->pCurrentChunk;
		pQueue->pCurrentChunk = pNewChunk;
	}

	ImAppEventQueueChunk* pChunk = pQueue->pCurrentChunk;

	pChunk->data[ pChunk->bottom ]	= *pEvent;
	pChunk->bottom = (pChunk->bottom + 1u) & (IMAPP_EVENT_QUEUE_CHUNK_SIZE - 1u);
	pChunk->length++;
}

bool ImAppEventQueuePop( ImAppEventQueue* pQueue, ImAppEvent* pEvent )
{
	if( pQueue->pCurrentChunk == NULL ||
		pQueue->pCurrentChunk->length == 0u )
	{
		return false;
	}

	ImAppEventQueueChunk* pChunk = pQueue->pCurrentChunk;

	*pEvent = pChunk->data[ pChunk->top ];

	pChunk->top = (pChunk->top + 1u) & (IMAPP_EVENT_QUEUE_CHUNK_SIZE - 1u);
	pChunk->length--;

	if( pChunk->length == 0u )
	{
		pQueue->pCurrentChunk = pChunk->pNext;
		ImUiMemoryFree( pQueue->pAllocator, pChunk );
	}

	return true;
}

#pragma once

#include "imapp/imapp.h"

typedef struct ImAppWindowDrop ImAppWindowDrop;
typedef struct ImAppWindowDrop
{
	ImAppWindowDrop*	nextDrop;

	ImAppDropType		type;
	char				pathOrText[ 1u ];
} ImAppWindowDrop;

typedef struct ImAppWindowDropQueue
{
	ImUiAllocator*		allocator;

    ImAppWindowDrop*    firstNew;
    ImAppWindowDrop*	firstPopped;
} ImAppWindowDropQueue;

//bool				ImAppWindowDropQueueConstruct( ImAppAllocator* allocator, ImAppWindowDropQueue* queue )

//bool				ImAppWindowDropQueuePush( ImAppWindowDropQueue* queue, ImAppDropType type, const char* )
//ImAppWindowDrop*	ImAppWindowDropQueuePop( ImAppWindowDropQueue* queue, ImAppDropQueue* queue )

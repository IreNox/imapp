#pragma once

#include "imapp/imapp.h"

typedef struct ImAppWindowDrop
{
	ImAppWindowDrop*	nextDrop;

	ImAppDropType		type;
	char				pathOrText[ 1u ];
} ImAppWindowDrop;

typedef ImAppWindowDropQueue
{
    ImAppWindowDrop*    firstNew;
    ImAppWindowDrop*	firstPopped;
} ImAppWindowDropQueue;

//bool				ImAppWindowDropQueuePush( ImAppAllocator* allocator, ImAppDropType type, const char* )
//ImAppWindowDrop*	ImAppWindowDropQueuePop( ImAppAllocator* allocator, ImAppDropQueue* queue )
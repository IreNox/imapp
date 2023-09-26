#pragma once

#include "imapp/imapp.h"

#include "imapp_event.h"

typedef struct ImAppEventQueue ImAppEventQueue;

ImAppEventQueue*	ImAppEventQueueCreate( ImUiAllocator* pAllocator );
void				ImAppEventQueueDestroy( ImAppEventQueue* pQueue );

void				ImAppEventQueuePush( ImAppEventQueue* pQueue, const ImAppEvent* pEvent );
bool				ImAppEventQueuePop( ImAppEventQueue* pQueue, ImAppEvent* pEvent );

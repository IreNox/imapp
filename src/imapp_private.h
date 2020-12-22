#pragma once

#include "imapp/imapp.h"

#include <stdbool.h>

struct ImAppPlatform;
typedef struct ImAppPlatform ImAppPlatform;

struct ImAppWindow;
typedef struct ImAppWindow ImAppWindow;

struct ImAppInput;
typedef struct ImAppInput ImAppInput;

struct ImAppRenderer;
typedef struct ImAppRenderer ImAppRenderer;

struct ImApp
{
	ImAppContext		context;		// must be at offset 0
	ImAppParameters		parameters;
	bool				running;

	ImAppPlatform*		pPlatform;
	ImAppWindow*		pWindow;
	ImAppRenderer*		pRenderer;

	void*				pProgramContext;

	struct nk_context	nkContext;
};
typedef struct ImApp ImApp;

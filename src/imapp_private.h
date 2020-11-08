#pragma once

#include "imapp/imapp.h"

#include <stdbool.h>

struct ImAppInput;
typedef struct ImAppInput ImAppInput;

struct ImAppRenderer;
typedef struct ImAppRenderer ImAppRenderer;

struct ImApp
{
	ImAppContext		context;		// must be at offset 0
	ImAppParameters		parameters;
	bool				running;

	void*				pProgramContext;
	SDL_Window*			pSdlWindow;
	struct nk_context	nkContext;

	ImAppInput*			pInput;
	ImAppRenderer*		pRenderer;
};
typedef struct ImApp ImApp;
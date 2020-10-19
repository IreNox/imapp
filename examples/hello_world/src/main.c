#include "imapp/imapp.h"

#include <stdlib.h>

struct ProgramContext
{
};

void* ImAppProgramInitialize( ImAppParameters* pParameters )
{
	pParameters->tickIntervalMs	= 15;
	pParameters->pWindowTitle	= "Hello World";
	pParameters->windowWidth	= 400;
	pParameters->windowHeight	= 200;

	ProgramContext* pContext = (ProgramContext*)malloc( sizeof( ProgramContext ) );
	return pContext;
}

void ImAppProgramTick( ImAppContext* pImAppContext, void* pProgramContext )
{

}

void ImAppProgramDoUi( ImAppContext* pImAppContext, void* pProgramContext )
{

}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
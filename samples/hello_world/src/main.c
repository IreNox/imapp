#include "imapp/imapp.h"

#include <stdio.h>
#include <stdlib.h>

struct ProgramContext
{
	int		tickIndex;
};

void* ImAppProgramInitialize( ImAppParameters* pParameters )
{
	pParameters->tickIntervalMs		= 15;
	pParameters->defaultFullWindow	= false;
	pParameters->pWindowTitle		= "Hello World";
	pParameters->windowWidth		= 400;
	pParameters->windowHeight		= 200;

	ProgramContext* pContext = (ProgramContext*)malloc( sizeof( ProgramContext ) );
	pContext->tickIndex = 0u;

	return pContext;
}

void ImAppProgramDoUi( ImAppContext* pImAppContext, void* pProgramContext )
{
	ProgramContext* pContext = (ProgramContext*)pProgramContext;
	struct nk_context* pNkContext = pImAppContext->pNkContext;

	if( nk_begin( pNkContext, "Hello World", nk_recti( pImAppContext->x, pImAppContext->y, pImAppContext->width, pImAppContext->height ), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE ) )
	{
		nk_layout_row_dynamic( pNkContext, 60.0f, 1 );
		{
			char buffer[ 64u ];
			sprintf( buffer, "Hello for the %d time.", pContext->tickIndex );

			nk_label( pNkContext, buffer, NK_TEXT_ALIGN_CENTERED );
		}

		nk_layout_row_dynamic( pNkContext, 0.0f, 5 );
		{
			nk_spacing( pNkContext, 2 );
			if( nk_button_label( pNkContext, "Exit" ) )
			{
				ImAppQuit( pImAppContext );
			}
		}

		nk_end( pNkContext );
	}

	pContext->tickIndex++;
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
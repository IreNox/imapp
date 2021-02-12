#include "imapp/imapp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ProgramContext
{
	int		tickIndex;
	char	nameBuffer[ 128u ];
};

void* ImAppProgramInitialize( ImAppParameters* pParameters )
{
	pParameters->tickIntervalMs		= 15;
	pParameters->defaultFullWindow	= false;
	pParameters->windowTitle		= "Hello World";
	pParameters->windowWidth		= 400;
	pParameters->windowHeight		= 250;

	ProgramContext* pContext = (ProgramContext*)malloc( sizeof( ProgramContext ) );
	pContext->tickIndex = 0u;
	strcpy( pContext->nameBuffer, "World" );

	return pContext;
}

void ImAppProgramDoUi( ImAppContext* pImAppContext, void* pProgramContext )
{
	ProgramContext* pContext = (ProgramContext*)pProgramContext;
	struct nk_context* pNkContext = pImAppContext->nkContext;

	if( nk_begin( pNkContext, "Hello World", nk_recti( pImAppContext->x, pImAppContext->y, pImAppContext->width, pImAppContext->height ), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE ) )
	{
		nk_layout_row_dynamic( pNkContext, 20.0f, 1 );
		nk_spacing( pNkContext, 1 );

		nk_layout_row_begin( pNkContext, NK_DYNAMIC, 30.0f, 2 );

		nk_layout_row_push( pNkContext, 0.1f );
		nk_label( pNkContext, "Name:", NK_TEXT_LEFT );
		nk_layout_row_push( pNkContext, 0.9f );
		nk_edit_string_zero_terminated( pNkContext, NK_EDIT_FIELD, pContext->nameBuffer, 128u, NULL );

		nk_layout_row_end( pNkContext );

		nk_layout_row_dynamic( pNkContext, 0.0f, 1 );
		nk_spacing( pNkContext, 1 );

		{
			char buffer[ 64u ];
			sprintf( buffer, "Hello %s for the %d time.", pContext->nameBuffer, pContext->tickIndex );

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

		nk_spacing( pNkContext, 7 );

		nk_end( pNkContext );
	}

	pContext->tickIndex++;
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
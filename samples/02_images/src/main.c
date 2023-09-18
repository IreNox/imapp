#include "imapp/imapp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ProgramContext
{
	nk_color	color;
};

void* ImAppProgramInitialize( ImAppParameters* pParameters )
{
	pParameters->tickIntervalMs		= 15;
	pParameters->defaultFullWindow	= false;
	pParameters->windowTitle		= "Images";
	pParameters->windowWidth		= 800;
	pParameters->windowHeight		= 300;

	ProgramContext* pContext = (ProgramContext*)malloc( sizeof( ProgramContext ) );
	pContext->color.r = 0xffu;
	pContext->color.g = 0xffu;
	pContext->color.b = 0xffu;
	pContext->color.a = 0xffu;

	return pContext;
}

void ImAppProgramDoUi( ImAppContext* pImAppContext, void* pProgramContext )
{
	ProgramContext* pContext = (ProgramContext*)pProgramContext;
	struct nk_context* pNkContext = pImAppContext->nkContext;

	if( nk_begin( pNkContext, "Image", nk_recti( pImAppContext->x, pImAppContext->y, pImAppContext->width, pImAppContext->height ), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE ) )
	{
		const struct nk_image image = ImAppImageGetBlocking( pImAppContext, "test.png" );
		const struct nk_color color = pContext->color;

		nk_layout_row_dynamic( pNkContext, image.h, 3 );
		nk_spacing( pNkContext, 1 );
		nk_image_color( pNkContext, image, color );
		
		nk_end( pNkContext );
	}

	pContext->color.a++;
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
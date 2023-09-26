#include "imapp/imapp.h"

#include "imapp/../../src/imapp_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ImAppTestProgramContext ImAppTestProgramContext;
struct ImAppTestProgramContext
{
	int		tickIndex;
	char	nameBuffer[ 128u ];
};

void* ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] )
{
	parameters->tickIntervalMs		= 15;
	parameters->windowTitle			= "I'm App - Hello World";
	parameters->windowWidth			= 400;
	parameters->windowHeight		= 250;

	ImAppTestProgramContext* pContext = (ImAppTestProgramContext*)malloc( sizeof( ImAppTestProgramContext ) );
	pContext->tickIndex = 0u;
	strncpy( pContext->nameBuffer, "World", sizeof( pContext->nameBuffer ) );

	return pContext;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImUiWindow* window )
{
	ImAppTestProgramContext* pContext = (ImAppTestProgramContext*)programContext;

	ImUiWidget* vLayout = ImUiWidgetBegin( window );
	ImUiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );

	ImUiToolboxLabel( window, IMUI_STR( "Hello World" ));
	ImUiToolboxButtonLabel( window, IMUI_STR( "Hello World" ) );

	ImUiWidgetEnd( vLayout );

	//ImUiDrawRectColor( window->rootWidget, ImUiRectCreate( 25.0f, 25.0f, 25.0f, 25.0f ), ImUiColorCreateWhite( 1.0f ) );

	//if( nk_begin( pNkContext, "Hello World", nk_recti( pImAppContext->x, pImAppContext->y, pImAppContext->width, pImAppContext->height ), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE ) )
	//{
	//	nk_layout_row_dynamic( pNkContext, 20.0f, 1 );
	//	nk_spacing( pNkContext, 1 );

	//	nk_layout_row_begin( pNkContext, NK_DYNAMIC, 30.0f, 2 );

	//	nk_layout_row_push( pNkContext, 0.1f );
	//	nk_label( pNkContext, "Name:", NK_TEXT_LEFT );
	//	nk_layout_row_push( pNkContext, 0.9f );
	//	nk_edit_string_zero_terminated( pNkContext, NK_EDIT_FIELD, pContext->nameBuffer, 128u, NULL );

	//	nk_layout_row_end( pNkContext );

	//	nk_layout_row_dynamic( pNkContext, 0.0f, 1 );
	//	nk_spacing( pNkContext, 1 );

	//	{
	//		char buffer[ 64u ];
	//		sprintf( buffer, "Hello %s for the %d time.", pContext->nameBuffer, pContext->tickIndex );

	//		nk_label( pNkContext, buffer, NK_TEXT_ALIGN_CENTERED );
	//	}

	//	nk_layout_row_dynamic( pNkContext, 0.0f, 5 );
	//	{
	//		nk_spacing( pNkContext, 2 );
	//		if( nk_button_label( pNkContext, "Exit" ) )
	//		{
	//			ImAppQuit( pImAppContext );
	//		}
	//	}

	//	nk_spacing( pNkContext, 7 );

	//	nk_end( pNkContext );
	//}

	pContext->tickIndex++;
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
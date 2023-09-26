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

	ImAppTestProgramContext* context = (ImAppTestProgramContext*)malloc( sizeof( ImAppTestProgramContext ) );
	context->tickIndex = 0u;
	strncpy( context->nameBuffer, "World", sizeof( context->nameBuffer ) );

	return context;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImUiWindow* window )
{
	ImAppTestProgramContext* context = (ImAppTestProgramContext*)programContext;

	ImUiWidget* vLayout = ImUiWidgetBegin( window );
	ImUiWidgetSetPadding( vLayout, ImUiBorderCreateAll( 8.0f ) );
	ImUiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );
	ImUiWidgetSetAlign( vLayout, ImUiAlignCreateCenter() );

	{
		ImUiWidget* hLayout = ImUiWidgetBegin( window );
		ImUiWidgetSetLayoutHorizontalSpacing( hLayout, 8.0f );

		ImUiToolboxLabel( window, IMUI_STR( "Name:" ) );
		ImUiToolboxLabel( window, IMUI_STR( context->nameBuffer ) );
		//	nk_edit_string_zero_terminated( pNkContext, NK_EDIT_FIELD, pContext->nameBuffer, 128u, NULL );

		ImUiWidgetEnd( hLayout );
	}

	ImUiToolboxLabelFormat( window, "Hello %s for the %d time.", context->nameBuffer, context->tickIndex );

	if( ImUiToolboxButtonLabel( window, IMUI_STR( "Exit" ) ) )
	{
		ImAppQuit( imapp );
	}

	ImUiWidgetEnd( vLayout );

	context->tickIndex++;
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
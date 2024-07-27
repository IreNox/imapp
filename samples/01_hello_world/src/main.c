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
	parameters->windowMode			= ImAppDefaultWindow_Resizable;

	ImAppTestProgramContext* context = (ImAppTestProgramContext*)malloc( sizeof( ImAppTestProgramContext ) );
	context->tickIndex = 0u;
	strncpy( context->nameBuffer, "World", sizeof( context->nameBuffer ) );

	return context;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImUiSurface* surface )
{
	ImAppTestProgramContext* context = (ImAppTestProgramContext*)programContext;

	ImUiWindow* window = ImUiWindowBegin( surface, "main", ImUiRectCreatePosSize( ImUiPosCreateZero(), ImUiSurfaceGetSize( surface ) ), 1u );

	ImUiWidget* vLayout = ImUiWidgetBegin( window );
	ImUiWidgetSetPadding( vLayout, ImUiBorderCreateAll( 8.0f ) );
	ImUiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );
	ImUiWidgetSetAlign( vLayout, 0.5f, 0.5f );

	{
		ImUiWidget* hLayout = ImUiWidgetBegin( window );
		ImUiWidgetSetHStretch( hLayout, 1.0f );
		ImUiWidgetSetLayoutHorizontalSpacing( hLayout, 8.0f );

		ImUiToolboxLabel( window, "Name:" );
		ImUiToolboxTextEdit( window, context->nameBuffer, sizeof( context->nameBuffer ), NULL );

		ImUiWidgetEnd( hLayout );
	}

	ImUiToolboxLabelFormat( window, "Hello %s for the %d time.", context->nameBuffer, context->tickIndex );

	if( ImUiToolboxButtonLabel( window, "Exit" ) )
	{
		ImAppQuit( imapp );
	}

	ImUiWidgetEnd( vLayout );

	ImUiWindowEnd( window );

	context->tickIndex++;
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
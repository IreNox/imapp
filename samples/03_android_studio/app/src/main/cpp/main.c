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
	parameters->windowMode			= ImAppDefaultWindow_Fullscreen;
    parameters->defaultFontSize     = 48u;

	ImAppTestProgramContext* context = (ImAppTestProgramContext*)malloc( sizeof( ImAppTestProgramContext ) );
	context->tickIndex = 0u;
	strncpy( context->nameBuffer, "World", sizeof( context->nameBuffer ) );

	return context;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow )
{
	ImAppTestProgramContext* context = (ImAppTestProgramContext*)programContext;

	ImUiWidget* vLayout = ImUiWidgetBegin( uiWindow );
	ImUiWidgetSetPadding( vLayout, ImUiBorderCreateAll( 8.0f ) );
	ImUiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );
	ImUiWidgetSetAlign( vLayout, 0.5f, 0.5f );

	{
		ImUiWidget* hLayout = ImUiWidgetBegin( uiWindow );
		ImUiWidgetSetHStretch( hLayout, 1.0f );
		ImUiWidgetSetLayoutHorizontalSpacing( hLayout, 8.0f );

		ImUiToolboxLabel( uiWindow, "Name:" );
		ImUiToolboxTextEdit( uiWindow, context->nameBuffer, sizeof( context->nameBuffer ), NULL );

		ImUiWidgetEnd( hLayout );
	}

	ImUiToolboxLabelFormat( uiWindow, "Hello %s for the %d time.", context->nameBuffer, context->tickIndex );

	if( ImUiToolboxButtonLabel( uiWindow, "Exit" ) )
	{
		ImAppQuit( imapp, 0 );
	}

	ImUiWidgetEnd( vLayout );

	context->tickIndex++;
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
#include "imapp/imapp.h"

#include "imapp/../../src/imapp_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ImappTestProgramContext
{
	int		tickIndex;
	char	nameBuffer[ 128u ];
} ImappTestProgramContext;

void* imappProgramInitialize( ImappParameters* parameters, int argc, char* argv[] )
{
	parameters->tickIntervalMs			= 15;
	parameters->defaultWindow.title		= "I'm App - Hello World";
	parameters->defaultWindow.width		= 400;
	parameters->defaultWindow.height	= 250;
    parameters->defaultFontSize     	= 16u;

	ImappTestProgramContext* context = (ImappTestProgramContext*)malloc( sizeof( ImappTestProgramContext ) );
	context->tickIndex = 0u;
	strncpy( context->nameBuffer, "World", sizeof( context->nameBuffer ) );

	return context;
}

void imappProgramDoDefaultWindowUi( ImappContext* imapp, void* programContext, ImappWindow* appWindow, ImuiWindow* uiWindow )
{
	ImappTestProgramContext* context = (ImappTestProgramContext*)programContext;

	ImuiWidget* vLayout = imuiWidgetBegin( uiWindow );
	imuiWidgetSetPadding( vLayout, imuiBorderCreateAll( 8.0f ) );
	imuiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );
	imuiWidgetSetAlign( vLayout, 0.5f, 0.5f );

	{
		ImuiWidget* hLayout = imuiWidgetBegin( uiWindow );
		imuiWidgetSetHStretch( hLayout, 1.0f );
		imuiWidgetSetLayoutHorizontalSpacing( hLayout, 8.0f );

		imuiToolboxLabel( uiWindow, "Name:" );
		imuiToolboxTextEdit( uiWindow, context->nameBuffer, sizeof( context->nameBuffer ), NULL );

		imuiWidgetEnd( hLayout );
	}

	imuiToolboxLabelFormat( uiWindow, "Hello %s for the %d time.", context->nameBuffer, context->tickIndex );

	if( imuiToolboxButtonLabel( uiWindow, "Exit" ) )
	{
		imappQuit( imapp, 0 );
	}

	imuiWidgetEnd( vLayout );

	context->tickIndex++;
}

void imappProgramShutdown( ImappContext* imappContext, void* programContext )
{
	free( programContext );
}
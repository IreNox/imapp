#include "imapp/imapp.h"

#include "imapp/../../src/imapp_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct ImAppMultiWindowsSampleContext
{
	ImAppWindow*				windows[ 10 ];
	size_t						windowCount;
} ImAppMultiWindowsSampleContext;

static void imappMultiWindowSecondWindowUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow, void* uiContext );

void* ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] )
{
	parameters->tickIntervalMs		= 15;
	parameters->defaultWindow.title	= "I'm App - Default Window";

	ImAppMultiWindowsSampleContext* context = (ImAppMultiWindowsSampleContext*)malloc( sizeof( ImAppMultiWindowsSampleContext ) );
	memset( context, 0, sizeof( *context ) );

	return context;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow )
{
	ImUiContext* imui = ImUiWindowGetContext( uiWindow );
	ImAppMultiWindowsSampleContext* context = (ImAppMultiWindowsSampleContext*)programContext;

	ImUiWidget* lMain = ImUiWidgetBegin( uiWindow );
	ImUiWidgetSetStretchOne( lMain );

	ImUiWidget* vLayout = ImUiWidgetBegin( uiWindow );
	ImUiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );
	ImUiWidgetSetAlign( vLayout, 0.5f, 0.5f );

	if( context->windowCount < 10 &&
		ImUiToolboxButtonLabel( uiWindow, "Open Window" ) )
	{
		char titleBuffer[ 64 ];
		snprintf( titleBuffer, sizeof( titleBuffer ), "Sub Window %zu", context->windowCount );

		ImAppWindowParameters winParams = { 0 };
		winParams.title		= titleBuffer;
		winParams.width		= 720;
		winParams.height	= 480;
		winParams.clearColor	= ImUiColorCreate( 0x18u, 0xa2u, 0x78u, 0xff );

		context->windows[ context->windowCount++ ] = ImAppWindowCreate( imapp, &winParams, imappMultiWindowSecondWindowUi, NULL );
	}

	for( size_t i = 0; i < context->windowCount; ++i )
	{
		if( !ImAppWindowIsOpen( imapp, context->windows[ i ] ) )
		{
			if( context->windowCount > 1 )
			{
				context->windows[ i ] = context->windows[ context->windowCount - 1 ];
			}
			context->windowCount--;
			i--;
			continue;
		}

		if( ImUiToolboxButtonLabelFormat( uiWindow, "Close Window %zu", i ) )
		{
			ImAppWindowDestroy( imapp, context->windows[ i ] );
		}
	}

	ImUiWidgetEnd( vLayout );
	ImUiWidgetEnd( lMain );
}

static void imappMultiWindowSecondWindowUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow, void* uiContext )
{
	ImUiWidget* vLayout = ImUiWidgetBegin( uiWindow );
	ImUiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );
	ImUiWidgetSetAlign( vLayout, 0.5f, 0.5f );

	{
		ImUiWidget* label = ImUiToolboxLabelBegin( uiWindow, "Hello from Sub Window" );
		ImUiWidgetSetHAlign( label, 0.5f );

		ImUiToolboxLabelEnd( label );
	}

	if( ImUiToolboxButtonLabel( uiWindow, "Close" ) )
	{
		ImAppWindowDestroy( imapp, appWindow );
	}

	ImUiWidgetEnd( vLayout );
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
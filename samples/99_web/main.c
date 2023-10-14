#include "imapp/imapp.h"

#include <stdlib.h>
#include <stdio.h>

static void HwInputElement( ImUiWindow* window );

void* ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] )
{
	return (void*)1;
}

void ImAppProgramShutdown( ImAppContext* imapp, void* programContext )
{
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImUiSurface* surface )
{
	ImUiContext* imui = ImUiSurfaceGetContext( surface );

	const ImUiSize surfaceSize = ImUiSurfaceGetSize( surface );
	ImUiWindow* window = ImUiWindowBegin( surface, ImUiStringViewCreate( "main" ), ImUiRectCreate( 0.0f, 0.0f, surfaceSize.width, surfaceSize.height ), 1 );

	ImUiWidget* cLayout = ImUiWidgetBeginNamed( window, ImUiStringViewCreate( "hMain" ) );
	ImUiWidgetSetStretch( cLayout, ImUiSizeCreate( 0.5f, 0.25f ) );
	ImUiWidgetSetAlign( cLayout, ImUiAlignCreateCenter() );

	ImUiDrawWidgetColor( cLayout, ImUiColorCreateWhite() );

	ImUiWidget* hLayout = ImUiWidgetBeginNamed( window, ImUiStringViewCreate( "vMain" ) );
	ImUiWidgetSetStretch( hLayout, ImUiSizeCreateOne() );
	ImUiWidgetSetPadding( hLayout, ImUiBorderCreateAll( 20.0f ) );
	ImUiWidgetSetLayoutHorizontalSpacing( hLayout, 20.0f );

	for( size_t i = 0u; i < 5u; ++i )
	{
		HwInputElement( window );
	}

	ImUiWidgetEnd( hLayout );

	ImUiWidgetEnd( cLayout );

	ImUiWindowEnd( window );
}

static void HwInputElement( ImUiWindow* window )
{
	ImUiWidget* widget = ImUiWidgetBegin( window );
	ImUiWidgetSetStretch( widget, ImUiSizeCreateOne() );

	ImUiColor color = ImUiColorCreateFloat( 0.1f, 0.5f, 0.8f, 1.0f );

	ImUiWidgetInputState inputState;
	ImUiWidgetGetInputState( widget, &inputState );

	if( inputState.hasMouseReleased )
	{
		color = ImUiColorCreateFloat( 1.0f, 0.3f, 0.2f, 1.0f );
	}
	else if( inputState.isMouseDown )
	{
		color = ImUiColorCreateFloat( 0.0f, 0.3f, 0.7f, 1.0f );
	}
	else if( inputState.isMouseOver )
	{
		color = ImUiColorCreateFloat( 0.5f, 0.9f, 1.0f, 1.0f );
	}

	ImUiDrawWidgetColor( widget, color );

	ImUiWidgetEnd( widget );
}

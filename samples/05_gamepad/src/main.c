#include "imapp/imapp.h"

#include "imapp/../../src/imapp_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef enum ImAppControllerSampleState
{
	ImAppControllerSampleState_MainMenu,
	ImAppControllerSampleState_NewGame,
	ImAppControllerSampleState_Options,
	ImAppControllerSampleState_Credits
} ImAppControllerSampleState;

typedef struct ImAppControllerSampleContext
{
	ImAppControllerSampleState	state;

	int		tickIndex;
	char	nameBuffer[ 128u ];
} ImAppControllerSampleContext;

static void ImAppControllerSampleMainMenu( ImAppContext* imapp, ImAppControllerSampleContext* context, ImUiWindow* window );
static void ImAppControllerSampleNewGame( ImAppControllerSampleContext* context, ImUiWindow* window );
static void ImAppControllerSampleOptions( ImAppControllerSampleContext* context, ImUiWindow* window );
static void ImAppControllerSampleCredits( ImUiWindow* window );

void* ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] )
{
	parameters->tickIntervalMs		= 15;
	parameters->windowTitle			= "I'm App - Controller";
	parameters->windowWidth			= 1280;
	parameters->windowHeight		= 720;
	parameters->windowMode			= ImAppDefaultWindow_Resizable;

	ImAppControllerSampleContext* context = (ImAppControllerSampleContext*)malloc( sizeof( ImAppControllerSampleContext ) );
	context->state = ImAppControllerSampleState_Options;
	//strncpy( context->nameBuffer, "World", sizeof( context->nameBuffer ) );

	return context;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow )
{
	ImUiContext* imui = ImUiWindowGetContext( uiWindow );
	ImAppControllerSampleContext* context = (ImAppControllerSampleContext*)programContext;

	ImUiPos focusPoint = ImUiPosCreateZero();
	const ImUiWidget* focusWidget = ImUiWindowGetFocusWidget( uiWindow );
	if( focusWidget )
	{
		focusPoint = ImUiRectGetCenter( ImUiWidgetGetRect( focusWidget ) );
	}

	ImUiWindowSetFocus( uiWindow, 0.5f, true );

	ImUiWidget* vMain = ImUiWidgetBeginId( uiWindow, context->state );
	ImUiWidgetSetStretchOne( vMain );

	switch( context->state )
	{
	case ImAppControllerSampleState_MainMenu:
		ImAppControllerSampleMainMenu( imapp, context, uiWindow );
		break;

	case ImAppControllerSampleState_NewGame:
		ImAppControllerSampleNewGame( context, uiWindow );
		break;

	case ImAppControllerSampleState_Options:
		ImAppControllerSampleOptions( context, uiWindow );
		break;

	case ImAppControllerSampleState_Credits:
		ImAppControllerSampleCredits( uiWindow );
		break;
	}

	if( ImUiInputGetShortcut( imui ) == ImUiInputShortcut_Back )
	{
		context->state = ImAppControllerSampleState_MainMenu;
	}

	const ImUiWidget* peekFocusWidget = ImUiWindowPeekFocusWidget( uiWindow );
	if( peekFocusWidget )
	{
		const ImUiRect focusRect = ImUiRectShrinkBorder( ImUiWidgetGetRect( peekFocusWidget ), ImUiBorderCreateAll( 4.0f ) );
		const float gray = (cosf( (float)ImUiWidgetGetTime( vMain ) ) / 2.0f + 0.5f) * 255.0f;
		ImUiWidgetDrawPartialColor( vMain, focusRect, ImUiColorCreateGray( (uint8_t)gray ) );
	}

	const ImUiPos p1 = ImUiPosAddPos( focusPoint, ImUiPosScale( ImUiInputGetDirection( imui ), 10000.0f ) );
	ImUiWidgetDrawLine( vMain, focusPoint, p1, ImUiColorCreateBlack() );

	ImUiWidgetEnd( vMain );
}

static const char* s_mainMenuButtons[] =
{
	"New Game",
	"Options",
	"Credits",
	"Exit"
};

static void ImAppControllerSampleMainMenu( ImAppContext* imapp, ImAppControllerSampleContext* context, ImUiWindow* window )
{
	ImUiWidget* vLayout = ImUiWidgetBegin( window );
	ImUiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );
	ImUiWidgetSetAlign( vLayout, 0.5f, 0.5f );

	for( int i = 0; i < IMAPP_ARRAY_COUNT( s_mainMenuButtons ); ++i )
	{
		ImUiWidget* button = ImUiToolboxButtonLabelBegin( window, s_mainMenuButtons[ i ] );
		ImUiWidgetSetHAlign( button, 0.5f );

		if( !ImUiWindowGetFocusWidget( window ) && i == 0 )
		{
			ImUiWidgetSetFocus( button );
		}

		if( ImUiToolboxButtonEnd( button ) )
		{
			if( i == ImAppControllerSampleState_Credits )
			{
				ImAppQuit( imapp, 0 );
			}
			else
			{
				context->state = i + 1;
			}
		}
	}

	ImUiWidgetEnd( vLayout );
}

static void ImAppControllerSampleNewGame( ImAppControllerSampleContext* context, ImUiWindow* window )
{
	ImUiWidget* vLayout = ImUiWidgetBegin( window );
	ImUiWidgetSetLayoutVerticalSpacing( vLayout, 20.0f );
	ImUiWidgetSetAlign( vLayout, 0.5f, 0.5f );

	ImUiToolboxLabel( window, "New Game" );

	ImUiWidget* hLayout = ImUiWidgetBegin( window );
	ImUiWidgetSetLayoutHorizontalSpacing( hLayout, 20.0f );

	for( int i = 0; i < 3u; ++i )
	{
		ImUiWidget* button = ImUiToolboxButtonBegin( window );
		ImUiWidgetSetPadding( button, ImUiBorderCreateAll( 8.0f ) );

		if( !ImUiWindowGetFocusWidget( window ) && i == 0 )
		{
			ImUiWidgetSetFocus( button );
		}

		ImUiWidget* buttonLayout = ImUiWidgetBegin( window );
		ImUiWidgetSetLayoutVerticalSpacing( buttonLayout, 20.0f );
		ImUiWidgetSetFixedSize( buttonLayout, ImUiSizeCreate( 125.0f, 200.0f ) );

		{
			ImUiWidget* title = ImUiToolboxLabelBeginFormat( window, "Save Game %i", i + 1u );
			ImUiWidgetSetHAlign( title, 0.5f );
			ImUiWidgetEnd( title );
		}

		{
			ImUiWidget* image = ImUiWidgetBegin( window );
			ImUiWidgetSetFixedSize( image, ImUiSizeCreate( 100.0f, 100.0f ) );
			ImUiWidgetSetHAlign( image, 0.5f );

			ImUiWidgetDrawColor( image, ImUiColorCreateGray( 128u ) );

			ImUiWidgetEnd( image );
		}

		{
			const uint32_t value = ((i * 3219687321968) ^ 498231984) % 101;
			ImUiWidget* progress = ImUiToolboxLabelBeginFormat( window, "Progress: %i%%", value );
			ImUiWidgetSetHAlign( progress, 0.5f );
			ImUiWidgetEnd( progress );
		}

		ImUiWidgetEnd( buttonLayout );

		ImUiToolboxButtonEnd( button );
	}

	ImUiWidgetEnd( hLayout );

	if( ImUiToolboxButtonLabel( window, "Back" ) )
	{
		context->state = ImAppControllerSampleState_MainMenu;
	}

	ImUiWidgetEnd( vLayout );
}

typedef enum ImAppControllerSampleOptionType
{
	ImAppControllerSampleOptionType_ValueSelect,
	ImAppControllerSampleOptionType_CheckBox,
	ImAppControllerSampleOptionType_Slider
} ImAppControllerSampleOptionType;

typedef struct ImAppControllerSampleOption
{
	const char*						text;
	ImAppControllerSampleOptionType	type;
} ImAppControllerSampleOption;

static ImAppControllerSampleOption s_options[] =
{
	{ "Resolution",		ImAppControllerSampleOptionType_CheckBox },
	{ "FPS limit",		ImAppControllerSampleOptionType_CheckBox },
	{ "VSync",			ImAppControllerSampleOptionType_CheckBox },
	{ "View distance",	ImAppControllerSampleOptionType_CheckBox },
	{ "Shadow quality",	ImAppControllerSampleOptionType_CheckBox },
	{ "Anti-aliasing",	ImAppControllerSampleOptionType_CheckBox }
};

static void ImAppControllerSampleOptions( ImAppControllerSampleContext* context, ImUiWindow* window )
{
	ImUiToolboxListContext list;
	ImUiWidget* listWidget = ImUiToolboxListBegin( &list, window, 24.0f, IMAPP_ARRAY_COUNT( s_options ), true );
	//ImUiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );
	ImUiWidgetSetAlign( listWidget, 0.5f, 0.5f );
	ImUiWidgetSetStretch( listWidget, 0.5f, 0.5f );

	for( size_t i = ImUiToolboxListGetBeginIndex( &list ); i < ImUiToolboxListGetEndIndex( &list ); ++i )
	{
		const ImAppControllerSampleOption* option = &s_options[ i ];

		ImUiWidget* optionWidget = ImUiToolboxListNextItem( &list );
		ImUiWidgetSetLayoutHorizontal( optionWidget );

		if( !ImUiWindowGetFocusWidget( window ) && i == 0 )
		{
			ImUiWidgetSetFocus( optionWidget );
		}

		ImUiToolboxLabel( window, option->text );
		ImUiToolboxStrecher( window, 1.0f, 0.0f );

		switch( option->type )
		{
		case ImAppControllerSampleOptionType_CheckBox:
			ImUiToolboxCheckBoxState( window, "Enable" );
			break;

		default:
			break;
		}
	}

	ImUiToolboxListEnd( &list );
}

static const char* s_creditsText[] =
{
	"Credits",
	"",
	"Programming by",
	"Tim Boden",
	"",
	"Design by",
	"Tim Boden",
	"",
	"Art by",
	"Tim Boden",
	"",
	"Third party libraries",
	"imui",
	"stb_truetype",
	"libspng"
};

typedef struct ImAppControllerSampleCreditsState
{
	double	lastTime;
	float	scrollOffset;
} ImAppControllerSampleCreditsState;

static void ImAppControllerSampleCredits( ImUiWindow* window )
{
	const double time = ImUiWindowGetTime( window );

	bool isNew;
	ImAppControllerSampleCreditsState* state = ImUiWindowAllocStateNew( window, sizeof( ImAppControllerSampleCreditsState ), 0, &isNew );
	if( isNew )
	{
		state->lastTime = time;
		state->scrollOffset = ImUiWindowGetRect( window ).size.height;
	}

	const float deltaTime = (float)(time - state->lastTime);
	state->lastTime = time;

	state->scrollOffset -= deltaTime * 50.0f;

	ImUiWidget* scrollLayout = ImUiWidgetBegin( window );
	ImUiWidgetSetLayoutScroll( scrollLayout, 0.0f, -state->scrollOffset );
	ImUiWidgetSetStretchOne( scrollLayout );

	ImUiWidget* vLayout = ImUiWidgetBegin( window );
	ImUiWidgetSetLayoutVerticalSpacing( vLayout, 8.0f );
	ImUiWidgetSetHAlign( vLayout, 0.5f );

	if( ImUiRectGetBottom( ImUiWidgetGetRect( vLayout ) ) < 0.0f )
	{
		state->scrollOffset = ImUiWindowGetRect( window ).size.height;
	}

	for( int i = 0; i < IMAPP_ARRAY_COUNT( s_creditsText ); ++i )
	{
		ImUiWidget* label = ImUiToolboxLabelBegin( window, s_creditsText[ i ] );
		ImUiWidgetSetHAlign( label, 0.5f );
		ImUiToolboxLabelEnd( label );
	}

	ImUiWidgetEnd( vLayout );
	ImUiWidgetEnd( scrollLayout );
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
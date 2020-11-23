#include "imapp/imapp.h"

#include "imapp_defines.h"
#include "imapp_helper.h"
#include "imapp_platform.h"
#include "imapp_renderer.h"

#include <limits.h>

static bool ImAppInitialize( ImApp* pImApp );
static bool ImAppRecreateRenderer( ImApp* pImApp );
static void ImAppCleanup( ImApp* pImApp );

int ImAppMain( ImAppPlatform* pPlatform, int argc, char* argv[] )
{
	ImApp* pImApp = IMAPP_NEW_ZERO( ImApp );

	pImApp->running				= true;
	pImApp->pPlatform			= pPlatform;
	pImApp->context.pNkContext	= &pImApp->nkContext;

	pImApp->parameters.tickIntervalMs		= 0;
	pImApp->parameters.defaultFullWindow	= true;
	pImApp->parameters.pWindowTitle			= "I'm App";
	pImApp->parameters.windowWidth			= 1280;
	pImApp->parameters.windowHeight			= 720;

	if( !ImAppInitialize( pImApp ) )
	{
		ImAppCleanup( pImApp );
		return 1;
	}

	int64_t lastTickValue = 0;
	while( pImApp->running )
	{
		lastTickValue = ImAppWindowTick( pImApp->pWindow, lastTickValue, pImApp->parameters.tickIntervalMs );

		pImApp->running &= ImAppWindowIsOpen( pImApp->pWindow );

		nk_input_begin( &pImApp->nkContext );
		ImAppInputApply( pImApp->pInput, &pImApp->nkContext );
		nk_input_end( &pImApp->nkContext );

		ImAppWindowGetSize( &pImApp->context.width, &pImApp->context.height, pImApp->pWindow );
		//pImApp->context.width /= 4u;
		//pImApp->context.height /= 4u;

		// UI
		{
			if( pImApp->parameters.defaultFullWindow )
			{
				nk_begin( &pImApp->nkContext, "Default", nk_recti( 0, 0, pImApp->context.width, pImApp->context.height ), NK_WINDOW_NO_SCROLLBAR );
			}

			ImAppProgramDoUi( &pImApp->context, pImApp->pProgramContext );

			if( pImApp->parameters.defaultFullWindow )
			{
				nk_end( &pImApp->nkContext );
			}
		}

		ImAppRendererDraw( pImApp->pRenderer, &pImApp->nkContext, pImApp->context.width, pImApp->context.height );
		if( !ImAppWindowPresent( pImApp->pWindow ) )
		{
			if( !ImAppRecreateRenderer( pImApp ) )
			{
				pImApp->running = false;
			}
		}
	}

	ImAppCleanup( pImApp );
	return 0;
}

static bool ImAppInitialize( ImApp* pImApp )
{
	pImApp->pProgramContext = ImAppProgramInitialize( &pImApp->parameters );
	if( pImApp->pProgramContext == NULL )
	{
		ImAppShowError( pImApp->pPlatform, "Failed to initialize Program." );
		return false;
	}

	pImApp->pWindow = ImAppWindowCreate( pImApp->pPlatform, pImApp->parameters.pWindowTitle, 0, 0, pImApp->parameters.windowWidth, pImApp->parameters.windowHeight, ImAppWindowState_Default );
	if( pImApp->pWindow == NULL )
	{
		ImAppShowError( pImApp->pPlatform, "Failed to create Window." );
		return false;
	}

	pImApp->pInput = ImAppInputCreate( pImApp->pPlatform, pImApp->pWindow );
	if( pImApp->pInput == NULL )
	{
		ImAppShowError( pImApp->pPlatform, "Failed to create Input." );
		return false;
	}

	pImApp->pRenderer = ImAppRendererCreate( pImApp->pPlatform );
	if( pImApp->pRenderer == NULL )
	{
		ImAppShowError( pImApp->pPlatform, "Failed to create Renderer." );
		return false;
	}

	{
		struct nk_font* pFont = ImAppRendererCreateDefaultFont( pImApp->pRenderer, &pImApp->nkContext );
		nk_init_default( &pImApp->nkContext, &pFont->handle );
	}

	return true;
}

static bool ImAppRecreateRenderer( ImApp* pImApp )
{
	if( pImApp->pRenderer != NULL )
	{
		ImAppRendererDestroy( pImApp->pRenderer );
		pImApp->pRenderer = NULL;
	}

	pImApp->pRenderer = ImAppRendererCreate( pImApp->pPlatform );
	if( pImApp->pRenderer == NULL )
	{
		ImAppShowError( pImApp->pPlatform, "Failed to create Renderer." );
		return false;
	}

	return true;
}

static void ImAppCleanup( ImApp* pImApp )
{
	if( pImApp->pProgramContext != NULL )
	{
		ImAppProgramShutdown( &pImApp->context, pImApp->pProgramContext );
		pImApp->pProgramContext = NULL;
	}

	if( pImApp->pRenderer != NULL )
	{
		ImAppRendererDestroy( pImApp->pRenderer );
		pImApp->pRenderer = NULL;
	}

	if( pImApp->pInput != NULL )
	{
		ImAppInputDestroy( pImApp->pInput );
		pImApp->pInput = NULL;
	}

	if( pImApp->pWindow != NULL )
	{
		ImAppWindowDestroy( pImApp->pWindow );
		pImApp->pWindow = NULL;
	}

	ImAppFree( pImApp );
}

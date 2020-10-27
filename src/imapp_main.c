#include "imapp/imapp.h"

#include "imapp_helper.h"
#include "imapp_renderer.h"

#include <SDL.h>
#include <limits.h>
#include <stdbool.h>

struct ImApp
{
	struct ImAppParameters	parameters;
	struct ImAppContext		context;
	bool					running;

	void*					pProgramContext;
	struct SDL_Window*		pSdlWindow;
	struct nk_context		nkContext;

	ImAppRenderer*			pRenderer;
};
typedef struct ImApp ImApp;

static bool ImAppInitialize( ImApp* pImApp );
static void ImAppCleanup( ImApp* pImApp );
static void ImAppShowError( ImApp* pImApp, const char* pMessage );
static void ImAppHandleEvent( ImApp* pImApp, const SDL_Event* pSdlEvent );

int main( int argc, char* argv[] )
{
	ImApp* pImApp = IMAPP_NEW_ZERO( ImApp );
	pImApp->parameters.tickIntervalMs	= 0;
	pImApp->parameters.pWindowTitle		= "I'm App";
	pImApp->parameters.windowWidth		= 1280;
	pImApp->parameters.windowHeight		= 720;

	if( !ImAppInitialize( pImApp ) )
	{
		ImAppCleanup( pImApp );
		return 1;
	}
	
	int remainingMs = 0;
	pImApp->running = true;
	while( pImApp->running )
	{
		const Uint32 startTick = SDL_GetTicks();

		int timeToWait = remainingMs;
		if( pImApp->parameters.tickIntervalMs == 0 )
		{
			timeToWait = INT_MAX;
		}

		if( SDL_WaitEventTimeout( NULL, timeToWait ) != 0 )
		{
			SDL_Event sdlEvent;
			while( SDL_PollEvent( &sdlEvent ) )
			{
				ImAppHandleEvent( pImApp, &sdlEvent );
			}
		}

		ImAppRendererFrame* pFrame = ImAppRendererBeginFrame( pImApp->pRenderer );

		const float color[ 4u ] = { 1.0f, 0.0f, 0.0f, 1.0f };
		ImAppRendererFrameClear( pFrame, color );

		ImAppRendererEndFrame( pImApp->pRenderer, pFrame );

		const Uint32 endTick = SDL_GetTicks();
		remainingMs = pImApp->parameters.tickIntervalMs - (endTick - startTick);
		remainingMs = remainingMs < 0 ? 0 : remainingMs;
	}

	ImAppCleanup( pImApp );
	return 0;
}

static bool ImAppInitialize( ImApp* pImApp )
{
	if( SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
	{
		ImAppShowError( pImApp, "Failed to initialize SDL." );
		return false;
	}

	pImApp->pProgramContext = ImAppProgramInitialize( &pImApp->parameters );
	if( pImApp->pProgramContext == NULL )
	{
		ImAppShowError( pImApp, "Failed to initialize Program." );
		return false;
	}

	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	windowFlags |= SDL_WINDOW_RESIZABLE;
#endif

	pImApp->pSdlWindow = SDL_CreateWindow( pImApp->parameters.pWindowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pImApp->parameters.windowWidth, pImApp->parameters.windowHeight, windowFlags );
	if( pImApp->pSdlWindow == NULL )
	{
		ImAppShowError( pImApp, "Failed to create Window." );
		return false;
	}

	pImApp->pRenderer = ImAppRendererCreate( pImApp->pSdlWindow );
	if( pImApp->pRenderer == NULL )
	{
		ImAppShowError( pImApp, "Failed to create Renderer." );
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

	if( pImApp->pSdlWindow != NULL )
	{
		SDL_DestroyWindow( pImApp->pSdlWindow );
		pImApp->pSdlWindow = NULL;
	}

	SDL_Quit();

	free( pImApp );
}

static void ImAppShowError( ImApp* pImApp, const char* pMessage )
{
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, pImApp->parameters.pWindowTitle, pMessage, pImApp->pSdlWindow );
}

void ImAppHandleEvent( ImApp* pImApp, const SDL_Event* pSdlEvent )
{
	switch( pSdlEvent->type )
	{
	case SDL_WINDOWEVENT:
		{
			const SDL_WindowEvent* pWindowEvent = &pSdlEvent->window;
			switch( pWindowEvent->event )
			{
			case SDL_WINDOWEVENT_CLOSE:
				pImApp->running = false;
				break;

			default:
				break;
			}
		}
		break;

	default:
		break;
	}
}

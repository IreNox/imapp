#include "imapp/imapp.h"

#include "imapp_helper.h"

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
};

void	ImAppHandleEvent( struct ImApp* pImApp, const SDL_Event* pSdlEvent );
void	ImAppCleanup( struct ImApp* pImApp );

int main( int argc, char* argv[] )
{
	struct ImApp* pImApp = IMAPP_NEW_ZERO( struct ImApp );
	pImApp->parameters.tickIntervalMs	= 0;
	pImApp->parameters.pWindowTitle		= "I'm App";
	pImApp->parameters.windowWidth		= 1280;
	pImApp->parameters.windowHeight		= 720;

	if( SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
	{
		return 1u;
	}

	pImApp->pProgramContext = ImAppProgramInitialize( &pImApp->parameters );
	if( pImApp->pProgramContext == NULL )
	{
		ImAppCleanup( pImApp );
		return 1u;
	}

	SDL_Window* pWindow = SDL_CreateWindow( pImApp->parameters.pWindowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pImApp->parameters.windowWidth, pImApp->parameters.windowHeight, 0u );
	if( pWindow == NULL )
	{
		ImAppCleanup( pImApp );
		return 1u;
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

		const Uint32 endTick = SDL_GetTicks();
		remainingMs = pImApp->parameters.tickIntervalMs - (endTick - startTick);
		remainingMs = remainingMs < 0 ? 0 : remainingMs;
	}

	ImAppCleanup( pImApp );
	return 0;
}

void ImAppHandleEvent( struct ImApp* pImApp, const SDL_Event* pSdlEvent )
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

void ImAppCleanup( struct ImApp* pImApp )
{
	if( pImApp->pProgramContext != NULL )
	{
		ImAppProgramShutdown( &pImApp->context, pImApp->pProgramContext );
		pImApp->pProgramContext = NULL;
	}

	if( pImApp->pSdlWindow != NULL )
	{
		SDL_DestroyWindow( pImApp->pSdlWindow );
		pImApp->pSdlWindow = NULL;
	}

	SDL_Quit();

	free( pImApp );
}

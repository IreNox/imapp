#include "imapp_renderer.h"

#include "imapp_helper.h"

//#include <GL/glew.h>
#include <SDL_opengl.h>

struct ImAppRendererFrame
{
	ImAppRenderer*		pRenderer;
};

struct ImAppRenderer
{
	SDL_Window*			pSdlWindow;
	SDL_GLContext		pGlContext;

	ImAppRendererFrame	frame;
};

ImAppRenderer* ImAppRendererCreate( SDL_Window* pWindow )
{
	IMAPP_ASSERT( pWindow != NULL );

	int driverIndex = -1;
	for( int i = 0u; i < SDL_GetNumRenderDrivers(); ++i )
	{
		SDL_RendererInfo info;
		IMAPP_VERIFY( SDL_GetRenderDriverInfo( i, &info ) == 0 );

		if( !(info.flags & SDL_RENDERER_ACCELERATED) )
		{
			continue;
		}
	}

	ImAppRenderer* pRenderer = IMAPP_NEW_ZERO( ImAppRenderer );
	pRenderer->pSdlWindow	= pWindow;

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );

	pRenderer->pGlContext = SDL_GL_CreateContext( pWindow );
	if( pRenderer->pGlContext == NULL )
	{
		ImAppRendererDestroy( pRenderer );
		return NULL;
	}

	if( SDL_GL_SetSwapInterval( 1 ) < 0 )
	{
		//printf( "Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError() );
	}

	return pRenderer;
}

void ImAppRendererDestroy( ImAppRenderer* pRenderer )
{
	if( pRenderer->pGlContext != NULL )
	{
		SDL_GL_DeleteContext( pRenderer->pGlContext );
		pRenderer->pGlContext = NULL;
	}

	free( pRenderer );
}

void ImAppRendererUpdate( ImAppRenderer* pRenderer )
{
	
}

ImAppRendererTexture* ImAppRendererTextureCreateFromFile( ImAppRenderer* pRenderer, const void* pFilename )
{
	//SDL_CreateTextureFromSurface
	return NULL;
}

ImAppRendererTexture* ImAppRendererTextureCreateFromMemory( ImAppRenderer* pRenderer, const void* pData, size_t width, size_t height )
{
	return NULL;
}

void ImAppRendererTextureDestroy( ImAppRenderer* pRenderer, ImAppRendererTexture* pTexture )
{
}

ImAppRendererFrame* ImAppRendererBeginFrame( ImAppRenderer* pRenderer )
{
	pRenderer->frame.pRenderer = pRenderer;

	return &pRenderer->frame;
}

void ImAppRendererEndFrame( ImAppRenderer* pRenderer, ImAppRendererFrame* pFrame )
{
	IMAPP_ASSERT( pFrame == &pRenderer->frame );
	IMAPP_ASSERT( pFrame->pRenderer == pRenderer );

	SDL_GL_SwapWindow( pRenderer->pSdlWindow );

	pFrame->pRenderer = NULL;
}

void ImAppRendererFrameClear( ImAppRendererFrame* pFrame, const float color[ 4u ] )
{
	glClearColor( color[ 0u ], color[ 1u ], color[ 2u ], color[ 3u ] );
	glClear( GL_COLOR_BUFFER_BIT );
}

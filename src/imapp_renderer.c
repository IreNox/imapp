#include "imapp_renderer.h"

#include "imapp_helper.h"

#include <gl/GL.h>

struct ImAppRenderer
{
	SDL_Window*		pSdlWindow;
	SDL_Renderer*	pSdlRenderer;
};

struct ImAppRenderer* ImAppRendererCreate( SDL_Window* pWindow )
{
	IMAPP_ASSERT( pWindow != nullptr );

	const int driverCount = SDL_GetNumRenderDrivers();
	for( int i = 0u; i < driverCount; ++i )
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
	pRenderer->pSdlRenderer	= SDL_CreateRenderer( pWindow, ..., ... );

	return pRenderer;
}

void ImAppRendererDestroy( struct ImAppRenderer* pRenderer )
{

}

void ImAppRendererUpdate( struct ImAppRenderer* pRenderer )
{

}

struct ImAppRendererTexture* ImAppRendererTextureCreateFromPng( struct ImAppRenderer* pRenderer, const void* pData, size_t dataSize )
{

}

struct ImAppRendererTexture* ImAppRendererTextureCreateFromJpeg( struct ImAppRenderer* pRenderer, const void* pData, size_t dataSize )
{

}

void ImAppRendererTextureDestroy( struct ImAppRenderer* pRenderer, struct ImAppRendererTexture* pTexture )
{

}

struct ImAppRendererFrame* ImAppRendererBeginFrame( struct ImAppRenderer* pRenderer )
{

}

void ImAppRendererEndFrame( struct ImAppRenderer* pRenderer, struct ImAppRendererFrame* pFrame )
{

}

void ImAppRendererFrameClear( struct ImAppRendererFrame* pFrame, float color[ 4u ] )
{

}

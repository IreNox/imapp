#pragma once

struct SDL_Window;
typedef struct SDL_Window SDL_Window;

struct ImAppRenderer;
typedef struct ImAppRenderer ImAppRenderer;

struct ImAppRendererFrame;
typedef struct ImAppRendererFrame ImAppRendererFrame;

struct ImAppRendererTexture;
typedef struct ImAppRendererTexture ImAppRendererTexture;

ImAppRenderer*			ImAppRendererCreate( SDL_Window* pWindow );
void					ImAppRendererDestroy( ImAppRenderer* pRenderer );

void					ImAppRendererUpdate( ImAppRenderer* pRenderer );

ImAppRendererTexture*	ImAppRendererTextureCreateFromFile( ImAppRenderer* pRenderer, const void* pFilename );
ImAppRendererTexture*	ImAppRendererTextureCreateFromMemory( ImAppRenderer* pRenderer, const void* pData, size_t width, size_t height );
void					ImAppRendererTextureDestroy( ImAppRenderer* pRenderer, ImAppRendererTexture* pTexture );

ImAppRendererFrame*		ImAppRendererBeginFrame( ImAppRenderer* pRenderer );
void					ImAppRendererEndFrame( ImAppRenderer* pRenderer, ImAppRendererFrame* pFrame );

void					ImAppRendererFrameClear( ImAppRendererFrame* pFrame, const float color[ 4u ] );

#pragma once

struct ImAppWindow;
typedef struct ImAppWindow ImAppWindow;

struct ImAppRenderer;
typedef struct ImAppRenderer ImAppRenderer;

struct ImAppRendererTexture;
typedef struct ImAppRendererTexture ImAppRendererTexture;

struct nk_context;
struct nk_font;

ImAppRenderer*			ImAppRendererCreate( ImAppPlatform* pPlatform );
void					ImAppRendererDestroy( ImAppRenderer* pRenderer );

struct nk_font*			ImAppRendererCreateDefaultFont( ImAppRenderer* pRenderer, struct nk_context* pNkContext );

ImAppRendererTexture*	ImAppRendererTextureCreateFromFile( ImAppRenderer* pRenderer, const void* pFilename );
ImAppRendererTexture*	ImAppRendererTextureCreateFromMemory( ImAppRenderer* pRenderer, const void* pData, size_t width, size_t height );
void					ImAppRendererTextureDestroy( ImAppRenderer* pRenderer, ImAppRendererTexture* pTexture );

void					ImAppRendererDraw( ImAppRenderer* pRenderer, struct nk_context* pNkContext, int width, int height );

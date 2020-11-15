#pragma once

struct ImAppWindow;
typedef struct ImAppWindow ImAppWindow;

struct ImAppRenderer;
typedef struct ImAppRenderer ImAppRenderer;

struct ImAppRendererTexture;
typedef struct ImAppRendererTexture ImAppRendererTexture;

struct nk_context;
struct nk_font;

ImAppRenderer*			ImAppRendererCreate( ImAppWindow* pWindow );
void					ImAppRendererDestroy( ImAppRenderer* pRenderer );

void					ImAppRendererUpdate( ImAppRenderer* pRenderer );
void					ImAppRendererGetTargetSize( int* pWidth, int* pHeight, ImAppRenderer* pRenderer );

struct nk_font*			ImAppRendererCreateDefaultFont( ImAppRenderer* pRenderer, struct nk_context* pNkContext );

ImAppRendererTexture*	ImAppRendererTextureCreateFromFile( ImAppRenderer* pRenderer, const void* pFilename );
ImAppRendererTexture*	ImAppRendererTextureCreateFromMemory( ImAppRenderer* pRenderer, const void* pData, size_t width, size_t height );
void					ImAppRendererTextureDestroy( ImAppRenderer* pRenderer, ImAppRendererTexture* pTexture );

void					ImAppRendererDrawFrame( ImAppRenderer* pRenderer, struct nk_context* pNkContext );

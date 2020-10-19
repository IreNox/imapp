#pragma once

struct ImAppRenderer;

struct ImAppRendererFrame;
struct ImAppRendererTexture;

ImAppRenderer*			ImAppRendererCreate( SDL_Window* pWindow );
void					ImAppRendererDestroy( ImAppRenderer* pRenderer );

void					ImAppRendererUpdate( ImAppRenderer* pRenderer );

ImAppRendererTexture*	ImAppRendererTextureCreateFromPng( ImAppRenderer* pRenderer, const void* pData, size_t dataSize );
ImAppRendererTexture*	ImAppRendererTextureCreateFromJpeg( ImAppRenderer* pRenderer, const void* pData, size_t dataSize );
void					ImAppRendererTextureDestroy( ImAppRenderer* pRenderer, ImAppRendererTexture* pTexture );

ImAppRendererFrame*		ImAppRendererBeginFrame( ImAppRenderer* pRenderer );
void					ImAppRendererEndFrame( ImAppRenderer* pRenderer, ImAppRendererFrame* pFrame );

void					ImAppRendererFrameClear( ImAppRendererFrame* pFrame, float color[ 4u ] );

#pragma once

struct ImAppRenderer;

struct ImAppRendererFrame;
struct ImAppRendererTexture;

struct ImAppRenderer*			ImAppRendererCreate( SDL_Window* pWindow );
void							ImAppRendererDestroy( struct ImAppRenderer* pRenderer );

void							ImAppRendererUpdate( struct ImAppRenderer* pRenderer );

struct ImAppRendererTexture*	ImAppRendererTextureCreateFromPng( struct ImAppRenderer* pRenderer, const void* pData, size_t dataSize );
struct ImAppRendererTexture*	ImAppRendererTextureCreateFromJpeg( struct ImAppRenderer* pRenderer, const void* pData, size_t dataSize );
void							ImAppRendererTextureDestroy( struct ImAppRenderer* pRenderer, struct ImAppRendererTexture* pTexture );

struct ImAppRendererFrame*		ImAppRendererBeginFrame( struct ImAppRenderer* pRenderer );
void							ImAppRendererEndFrame( struct ImAppRenderer* pRenderer, struct ImAppRendererFrame* pFrame );

void							ImAppRendererFrameClear( struct ImAppRendererFrame* pFrame, float color[ 4u ] );

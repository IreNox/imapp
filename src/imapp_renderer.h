#pragma once

#include <imui/imui.h>

#include <stdbool.h>

typedef struct ImAppPlatform ImAppPlatform;
typedef struct ImAppRenderer ImAppRenderer;
typedef struct ImAppRendererTexture ImAppRendererTexture;
typedef struct ImAppWindow ImAppWindow;
typedef struct ImUiAllocator ImUiAllocator;
typedef struct ImUiDrawData ImUiDrawData;

typedef enum ImAppRendererFormat ImAppRendererFormat;
enum ImAppRendererFormat
{
	ImAppRendererFormat_R8,
	ImAppRendererFormat_RGB8,
	ImAppRendererFormat_RGBA8
};

ImUiVertexFormat		ImAppRendererGetVertexFormat();

ImAppRenderer*			ImAppRendererCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppWindow* window, ImUiColor clearColor );
void					ImAppRendererDestroy( ImAppRenderer* renderer );

bool					ImAppRendererCreateResources( ImAppRenderer* renderer );
void					ImAppRendererDestroyResources( ImAppRenderer* renderer );

ImAppRendererTexture*	ImAppRendererTextureCreate( ImAppRenderer* renderer );
ImAppRendererTexture*	ImAppRendererTextureCreateFromMemory( ImAppRenderer* renderer, const void* data, int width, int height, ImAppRendererFormat format, uint8_t flags );
bool					ImAppRendererTextureInitializeDataFromMemory( ImAppRenderer* renderer, ImAppRendererTexture* texture, const void* data, int width, int height, ImAppRendererFormat format, uint8_t flags );
void					ImAppRendererTextureDestroyData( ImAppRenderer* renderer, ImAppRendererTexture* texture );
void					ImAppRendererTextureDestroy( ImAppRenderer* renderer, ImAppRendererTexture* texture );

void					ImAppRendererDraw( ImAppRenderer* renderer, ImAppWindow* window, ImUiSurface* surface );

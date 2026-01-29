#pragma once

#include "imapp_types.h"

#include <imui/imui.h>

#include <stdbool.h>

typedef struct ImAppPlatform ImAppPlatform;
typedef struct ImAppRenderer ImAppRenderer;
typedef struct ImAppRendererTexture ImAppRendererTexture;
typedef struct ImAppWindow ImAppWindow;
typedef struct ImUiAllocator ImUiAllocator;
typedef struct ImUiDrawData ImUiDrawData;
typedef struct ImAppRendererWindow ImAppRendererWindow;

typedef enum ImAppRendererFormat ImAppRendererFormat;
enum ImAppRendererFormat
{
	ImAppRendererFormat_R8,
	ImAppRendererFormat_RGB8,
	ImAppRendererFormat_RGBA8
};

struct ImAppRendererWindow
{
	unsigned int				vertexArray;
	unsigned int				vertexBuffer;
	uintsize					vertexBufferSize;
	void*						vertexBufferData;
	unsigned int				elementBuffer;
	uintsize					elementBufferSize;
	void*						elementBufferData;
};

ImUiVertexFormat		imappRendererGetVertexFormat();

ImAppRenderer*			imappRendererCreate( ImUiAllocator* allocator, ImAppPlatform* platform );
void					imappRendererDestroy( ImAppRenderer* renderer );

bool					imappRendererCreateResources( ImAppRenderer* renderer );
void					imappRendererDestroyResources( ImAppRenderer* renderer );

void					imappRendererConstructWindow( ImAppRenderer* renderer, ImAppRendererWindow* window );
void					imappRendererDestructWindow( ImAppRenderer* renderer, ImAppRendererWindow* window );

ImAppRendererTexture*	imappRendererTextureCreate( ImAppRenderer* renderer );
ImAppRendererTexture*	imappRendererTextureCreateFromMemory( ImAppRenderer* renderer, const void* data, uint32_t width, uint32_t height, ImAppRendererFormat format, uint8_t flags );
bool					imappRendererTextureInitializeDataFromMemory( ImAppRenderer* renderer, ImAppRendererTexture* texture, const void* data, uint32_t width, uint32_t height, ImAppRendererFormat format, uint8_t flags );
void					imappRendererTextureDestroyData( ImAppRenderer* renderer, ImAppRendererTexture* texture );
void					imappRendererTextureDestroy( ImAppRenderer* renderer, ImAppRendererTexture* texture );

void					imappRendererDraw( ImAppRenderer* renderer, ImAppRendererWindow* window, ImUiSurface* surface, int width, int height, float clearColor[ 4 ] );

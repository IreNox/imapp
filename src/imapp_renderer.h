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

typedef enum ImAppRendererShading ImAppRendererShading;
enum ImAppRendererShading
{
	ImAppRendererShading_Opaque,
	ImAppRendererShading_Translucent,
	ImAppRendererShading_Font
};

ImUiVertexFormat		ImAppRendererGetVertexFormat();

ImAppRenderer*			ImAppRendererCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppWindow* window );
void					ImAppRendererDestroy( ImAppRenderer* renderer );

bool					ImAppRendererRecreateResources( ImAppRenderer* renderer );

ImAppRendererTexture*	ImAppRendererTextureCreateFromMemory( ImAppRenderer* renderer, const void* data, int width, int height, ImAppRendererFormat format, ImAppRendererShading shading );
void					ImAppRendererTextureDestroy( ImAppRenderer* renderer, ImAppRendererTexture* texture );

void					ImAppRendererDraw( ImAppRenderer* renderer, ImAppWindow* window, const ImUiDrawData* drawData );

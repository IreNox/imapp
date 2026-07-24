#pragma once

#include "imapp_types.h"

#include <imui/imui.h>

#include <stdbool.h>

typedef struct ImappPlatform ImappPlatform;
typedef struct ImappRenderer ImappRenderer;
typedef struct ImappRendererTexture ImappRendererTexture;
typedef struct ImappWindow ImappWindow;
typedef struct ImuiAllocator ImuiAllocator;
typedef struct ImuiDrawData ImuiDrawData;
typedef struct ImappRendererWindow ImappRendererWindow;

typedef enum ImappRendererFormat ImappRendererFormat;
enum ImappRendererFormat
{
	ImappRendererFormat_R8,
	ImappRendererFormat_RGB8,
	ImappRendererFormat_RGBA8
};

struct ImappRendererWindow
{
	unsigned int				vertexArray;
	unsigned int				vertexBuffer;
	uintsize					vertexBufferSize;
	void*						vertexBufferData;
	unsigned int				elementBuffer;
	uintsize					elementBufferSize;
	void*						elementBufferData;
};

ImuiVertexFormat		imappRendererGetVertexFormat();

ImappRenderer*			imappRendererCreate( ImuiAllocator* allocator, ImappPlatform* platform );
void					imappRendererDestroy( ImappRenderer* renderer );

void					imappRendererUpdate( ImappRenderer* renderer );

bool					imappRendererCreateResources( ImappRenderer* renderer );
void					imappRendererDestroyResources( ImappRenderer* renderer );

void					imappRendererConstructWindow( ImappRenderer* renderer, ImappRendererWindow* window );
void					imappRendererDestructWindow( ImappRenderer* renderer, ImappRendererWindow* window );

ImappRendererTexture*	imappRendererTextureCreate( ImappRenderer* renderer );
ImappRendererTexture*	imappRendererTextureCreateFromMemory( ImappRenderer* renderer, const void* data, uint32_t width, uint32_t height, ImappRendererFormat format, uint8_t flags );
bool					imappRendererTextureInitializeDataFromMemory( ImappRenderer* renderer, ImappRendererTexture* texture, const void* data, uint32_t width, uint32_t height, ImappRendererFormat format, uint8_t flags );
void					imappRendererTextureDestroyData( ImappRenderer* renderer, ImappRendererTexture* texture );
void					imappRendererTextureDestroy( ImappRenderer* renderer, ImappRendererTexture* texture );

void					imappRendererDraw( ImappRenderer* renderer, ImappRendererWindow* window, ImuiSurface* surface, int width, int height, float clearColor[ 4 ] );

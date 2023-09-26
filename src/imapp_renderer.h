#pragma once

typedef struct ImAppRenderer ImAppRenderer;
typedef struct ImAppRendererTexture ImAppRendererTexture;
typedef struct ImAppWindow ImAppWindow;
typedef struct ImUiAllocator ImUiAllocator;
typedef struct ImUiDrawData ImUiDrawData;

ImAppRenderer*			ImAppRendererCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppWindow* window );
void					ImAppRendererDestroy( ImAppRenderer* renderer );

bool					ImAppRendererRecreateResources( ImAppRenderer* renderer );

ImAppRendererTexture*	ImAppRendererTextureCreateFromMemory( ImAppRenderer* renderer, const void* data, int width, int height );
void					ImAppRendererTextureDestroy( ImAppRenderer* renderer, ImAppRendererTexture* texture );

void					ImAppRendererDraw( ImAppRenderer* renderer, ImAppWindow* window, const ImUiDrawData* drawData );

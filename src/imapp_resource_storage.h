#pragma once

#include "imapp_defines.h"

#include <imui/imui.h>

#include <stdbool.h>

typedef struct ImAppPlatform ImAppPlatform;
typedef struct ImAppRenderer ImAppRenderer;
typedef struct ImAppRendererTexture ImAppRendererTexture;
typedef struct ImAppResourceStorage ImAppResourceStorage;
typedef struct ImUiAllocator ImUiAllocator;
typedef struct ImUiContext ImUiContext;

typedef struct ImAppImage ImAppImage;
struct ImAppImage
{
	ImUiStringView			resourceName;

	ImAppRendererTexture*	pTexture;
	ImUiSize				size;
};

ImAppResourceStorage*		ImAppResourceStorageCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppRenderer* renderer, ImUiContext* imui );
void						ImAppResourceStorageDestroy( ImAppResourceStorage* storage );

void						ImAppResourceStorageUpdate( ImAppResourceStorage* storage );
bool						ImAppResourceStorageRecreateEverything( ImAppResourceStorage* storage );

ImAppImage*					ImAppResourceStorageImageFindOrLoad( ImAppResourceStorage* storage, ImUiStringView resourceName, bool autoFree );
ImAppImage*					ImAppResourceStorageImageCreateRaw( ImAppResourceStorage* storage, const void* pixelData, int width, int height );
ImAppImage*					ImAppResourceStorageImageCreatePng( ImAppResourceStorage* storage, const void* imageData, size_t imageDataSize );
//ImAppImage*					ImAppResourceStorageImageCreateJpeg( ImAppResourceStorage* storage, const void* imageData, size_t imageDataSize );
void						ImAppResourceStorageImageFree( ImAppResourceStorage* storage, ImAppImage* pImage );

ImUiFont*					ImAppResourceStorageFontCreate( ImAppResourceStorage* storage, ImUiStringView fontName, float fontSize );

#pragma once

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

ImAppImage*					ImAppResourceStorageImageFindOrLoad( ImAppResourceStorage* storage, ImUiStringView resourceName, bool autoFree );
ImAppImage*					ImAppResourceStorageImageCreateFromMemory( ImAppResourceStorage* storage, const void* pPixelData, int width, int height );
void						ImAppResourceStorageImageFree( ImAppResourceStorage* storage, ImAppImage* pImage );

ImUiFont*					ImAppResourceStorageFontCreate( ImAppResourceStorage* storage, ImUiStringView fontName, float fontSize );

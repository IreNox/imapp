#pragma once

typedef struct ImAppPlatform ImAppPlatform;
typedef struct ImAppRenderer ImAppRenderer;
typedef struct ImAppRendererTexture ImAppRendererTexture;
typedef struct ImAppImage ImAppImage;
typedef struct ImAppImageStorage ImAppImageStorage;

struct ImAppImage
{
	ImAppRendererTexture*	pTexture;
	int						width;
	int						height;
};

ImAppImageStorage*			ImAppImageStorageCreate( ImAppAllocator* pAllocator, ImAppPlatform* pPlatform, ImAppRenderer* pRenderer );
void						ImAppImageStorageDestroy( ImAppImageStorage* pStorage );

void						ImAppImageStorageUpdate( ImAppImageStorage* pStorage );

ImAppImage*					ImAppImageStorageFindImageOrLoad( ImAppImageStorage* pStorage, const char* pResourceName, bool autoFree );
ImAppImage*					ImAppImageStorageCreateImageFromMemory( ImAppImageStorage* pStorage, const void* pPixelData, int width, int height );
void						ImAppImageStorageFreeImage( ImAppImageStorage* pStorage, ImAppImage* pImage );

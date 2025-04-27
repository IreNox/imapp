#pragma once

#include "imapp/imapp.h"

#include "imapp_types.h"

#include <stdbool.h>

typedef struct ImAppPlatform ImAppPlatform;
typedef struct ImAppRenderer ImAppRenderer;
typedef struct ImAppRendererTexture ImAppRendererTexture;
typedef struct ImAppResSys ImAppResSys;
typedef struct ImUiAllocator ImUiAllocator;
typedef struct ImUiContext ImUiContext;

struct ImAppImage
{
	ImUiStringView			resourceName;

	ImUiImage				uiImage;
	ImAppBlob				data;
	ImAppResState			state;
};

typedef struct ImAppFont ImAppFont;
struct ImAppFont
{
	ImAppFont*				prevFont;
	ImAppFont*				nextFont;

	ImUiStringView			name;
	float					size;

	ImUiFont*				uiFont;
	ImAppRendererTexture*	texture;
};

ImAppResSys*	ImAppResSysCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppRenderer* renderer, ImUiContext* imui );
void			ImAppResSysDestroy( ImAppResSys* ressys );

void			ImAppResSysUpdate( ImAppResSys* ressys, bool wait );

void			ImAppResSysDestroyDeviceResources( ImAppResSys* ressys );
void			ImAppResSysCreateDeviceResources( ImAppResSys* ressys );

ImAppResPak*	ImAppResSysAdd( ImAppResSys* ressys, const void* pakData, uintsize dataLength );
ImAppResPak*	ImAppResSysOpen( ImAppResSys* ressys, const char* resourceName );
void			ImAppResSysClose( ImAppResSys* ressys, ImAppResPak* respak );

ImAppImage*		ImAppResSysImageCreateRaw( ImAppResSys* ressys, const void* pixelData, int width, int height );
ImAppImage*		ImAppResSysImageCreatePng( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize );
ImAppImage*		ImAppResSysImageCreateJpeg( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize );
ImAppImage*		ImAppResSysImageLoadResource( ImAppResSys* ressys, const char* resourceName );
ImAppResState	ImAppResSysImageGetState( ImAppResSys* ressys, ImAppImage* image );
void			ImAppResSysImageFree( ImAppResSys* ressys, ImAppImage* image );

ImAppFont*		ImAppResSysFontCreateSystem( ImAppResSys* ressys, const char* fontName, float fontSize );
void			ImAppResSysFontDestroy( ImAppResSys* ressys, ImAppFont* font );
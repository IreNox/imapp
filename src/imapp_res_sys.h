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

ImAppResSys*	imappResSysCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppRenderer* renderer, ImUiContext* imui );
void			imappResSysDestroy( ImAppResSys* ressys );

void			imappResSysUpdate( ImAppResSys* ressys, bool wait );

void			imappResSysDestroyDeviceResources( ImAppResSys* ressys );
void			imappResSysCreateDeviceResources( ImAppResSys* ressys );

ImAppResPak*	imappResSysAdd( ImAppResSys* ressys, const void* pakData, uintsize dataLength );
ImAppResPak*	imappResSysOpen( ImAppResSys* ressys, const char* resourceName );
void			imappResSysClose( ImAppResSys* ressys, ImAppResPak* respak );

ImAppImage*		imappResSysImageCreateRaw( ImAppResSys* ressys, const void* pixelData, int width, int height );
ImAppImage*		imappResSysImageCreatePng( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize );
ImAppImage*		imappResSysImageCreateJpeg( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize );
ImAppImage*		imappResSysImageLoadResource( ImAppResSys* ressys, const char* resourceName );
ImAppResState	imappResSysImageGetState( ImAppResSys* ressys, ImAppImage* image );
void			imappResSysImageFree( ImAppResSys* ressys, ImAppImage* image );

ImAppFont*		imappResSysFontCreateSystem( ImAppResSys* ressys, const char* fontName, float fontSize );
void			imappResSysFontDestroy( ImAppResSys* ressys, ImAppFont* font );
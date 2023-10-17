#pragma once

#include "imapp_defines.h"

#include <imui/imui.h>

#include <stdbool.h>

typedef struct ImAppPlatform ImAppPlatform;
typedef struct ImAppRenderer ImAppRenderer;
typedef struct ImAppRendererTexture ImAppRendererTexture;
typedef struct ImAppResSys ImAppResSys;
typedef struct ImUiAllocator ImUiAllocator;
typedef struct ImUiContext ImUiContext;

struct ImAppImage
{
	ImAppRendererTexture*	texture;
	uint32_t				width;
	uint32_t				height;

	char					resourceName[ 1u ];
};

ImAppResSys*	ImAppResSysCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppRenderer* renderer, ImUiContext* imui );
void			ImAppResSysDestroy( ImAppResSys* ressys );

void			ImAppResSysUpdate( ImAppResSys* ressys );
bool			ImAppResSysRecreateEverything( ImAppResSys* ressys );

ImAppResPak*	ImAppResSysOpen( ImAppResSys* ressys, const char* resourceName );
void			ImAppResSysClose( ImAppResSys* ressys, ImAppResPak* respak );

ImAppImage*		ImAppResSysImageCreateResource( ImAppResSys* ressys, const char* resourceName );
ImAppImage*		ImAppResSysImageCreateRaw( ImAppResSys* ressys, const void* pixelData, int width, int height );
ImAppImage*		ImAppResSysImageCreatePng( ImAppResSys* ressys, const void* imageData, size_t imageDataSize );
//ImAppImage*		ImAppResSysImageCreateJpeg( ImAppResSys* ressys, const void* imageData, size_t imageDataSize );
bool			ImAppResSysImageIsLoaded( ImAppResSys* ressys, ImAppImage* image );
void			ImAppResSysImageFree( ImAppResSys* ressys, ImAppImage* image );

ImUiFont*		ImAppResSysFontCreateSystem( ImAppResSys* ressys, const char* fontName, float fontSize );

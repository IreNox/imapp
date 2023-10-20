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
	ImUiImage				data;
	ImAppResState			state;

	char					resourceName[ 1u ];
};

ImAppResSys*	ImAppResSysCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppRenderer* renderer, ImUiContext* imui );
void			ImAppResSysDestroy( ImAppResSys* ressys );

void			ImAppResSysUpdate( ImAppResSys* ressys );
bool			ImAppResSysRecreateEverything( ImAppResSys* ressys );

ImAppResPak*	ImAppResSysOpen( ImAppResSys* ressys, const char* resourceName );
void			ImAppResSysClose( ImAppResSys* ressys, ImAppResPak* respak );

ImAppImage*		ImAppResSysImageCreateRaw( ImAppResSys* ressys, const void* pixelData, int width, int height );
ImAppImage*		ImAppResSysImageCreateResource( ImAppResSys* ressys, const char* resourceName );
ImAppImage*		ImAppResSysImageCreatePng( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize );
ImAppImage*		ImAppResSysImageCreateJpeg( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize );
ImAppResState	ImAppResSysImageGetState( ImAppResSys* ressys, ImAppImage* image );
void			ImAppResSysImageFree( ImAppResSys* ressys, ImAppImage* image );

ImUiFont*		ImAppResSysFontCreateSystem( ImAppResSys* ressys, const char* fontName, float fontSize, ImAppRendererTexture** texture );

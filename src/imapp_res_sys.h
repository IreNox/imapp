#pragma once

#include "imapp/imapp.h"

#include "imapp_types.h"

#include <stdbool.h>

typedef struct ImappPlatform ImappPlatform;
typedef struct ImappRenderer ImappRenderer;
typedef struct ImappRendererTexture ImappRendererTexture;
typedef struct ImappResSys ImappResSys;
typedef struct ImuiAllocator ImuiAllocator;
typedef struct ImuiContext ImuiContext;

struct ImappImage
{
	ImuiStringView			resourceName;

	ImuiImage				uiImage;
	ImappBlob				data;
	ImappResState			state;
};

typedef struct ImappFont ImappFont;
struct ImappFont
{
	ImappFont*				prevFont;
	ImappFont*				nextFont;

	ImuiStringView			name;
	float					size;

	ImuiFont*				uiFont;
	ImappRendererTexture*	texture;
};

ImappResSys*	imappResSysCreate( ImuiAllocator* allocator, ImappPlatform* platform, ImappRenderer* renderer, ImuiContext* imui );
void			imappResSysDestroy( ImappResSys* ressys );

void			imappResSysUpdate( ImappResSys* ressys, bool wait );

void			imappResSysDestroyDeviceResources( ImappResSys* ressys );
void			imappResSysCreateDeviceResources( ImappResSys* ressys );

ImappResPak*	imappResSysAdd( ImappResSys* ressys, const void* pakData, uintsize dataLength );
ImappResPak*	imappResSysOpen( ImappResSys* ressys, const char* resourceName );
void			imappResSysClose( ImappResSys* ressys, ImappResPak* respak );

ImappImage*		imappResSysImageCreateRaw( ImappResSys* ressys, const void* pixelData, int width, int height );
ImappImage*		imappResSysImageCreatePng( ImappResSys* ressys, const void* imageData, uintsize imageDataSize );
ImappImage*		imappResSysImageCreateJpeg( ImappResSys* ressys, const void* imageData, uintsize imageDataSize );
ImappImage*		imappResSysImageLoadResource( ImappResSys* ressys, const char* resourceName );
ImappResState	imappResSysImageGetState( ImappResSys* ressys, ImappImage* image );
void			imappResSysImageFree( ImappResSys* ressys, ImappImage* image );

ImappFont*		imappResSysFontCreateSystem( ImappResSys* ressys, const char* fontName, float fontSize );
void			imappResSysFontDestroy( ImappResSys* ressys, ImappFont* font );
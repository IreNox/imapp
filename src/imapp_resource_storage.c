#include "imapp_resource_storage.h"

#include "imapp_defines.h"
#include "imapp_platform.h"
#include "imapp_renderer.h"

#include <lodepng.h>
#include <stdlib.h>

typedef enum ImAppResourceState ImAppResourceState;
enum ImAppResourceState
{
	ImAppImageState_Invalid,
	ImAppImageState_Default,
	ImAppImageState_Managed,
	ImAppImageState_Unused
};

typedef struct ImAppResource ImAppResource;
struct ImAppResource
{
	ImAppImage			image;

	ImAppResource*		prevResource;
	ImAppResource*		nextResource;
	ImAppResourceState		state;

	char				resourceName[ 1u ];
};

typedef struct ImAppResourceList ImAppResourceList;
struct ImAppResourceList
{
	ImAppResource*		firstResource;
	ImAppResource*		lastResource;
};

struct ImAppResourceStorage
{
	ImUiAllocator*		allocator;
	ImAppPlatform*		platform;
	ImAppRenderer*		renderer;
	ImUiContext*		imui;

	ImAppResourceList	defaultImages;
	ImAppResourceList	managedImages;
	ImAppResourceList	unusedImages;

	ImUiHashMap			imageMap;
};

static void				ImAppResourceStorageChangeState( ImAppResourceStorage* storage, ImAppResource* image, ImAppResourceState state );
static void				ImAppResourceStorageFreeImageInternal( ImAppResourceStorage* storage, ImAppResource* image );

static void				ImAppResourceListAdd( ImAppResourceList* list, ImAppResource* resource );
static void				ImAppResourceListRemove( ImAppResourceList* list, ImAppResource* resource );

static ImUiHash ImAppStorageMapImageHash( const void* key )
{
	const ImAppResource* image = *(const ImAppResource**)key;
	return ImUiHashString( image->image.resourceName, 0u );
}

static bool ImAppStorageMapIsKeyEquals( const void* lhs, const void* rhs )
{
	const ImAppResource* lhsImage = *(const ImAppResource**)lhs;
	const ImAppResource* rhsImage = *(const ImAppResource**)rhs;

	if( lhsImage->image.resourceName.length != rhsImage->image.resourceName.length )
	{
		return false;
	}

	if( lhsImage->image.resourceName.length == 0u )
	{
		return true;
	}

	if( lhsImage->image.resourceName.data[ 0u ] != rhsImage->image.resourceName.data[ 0u ] )
	{
		return false;
	}

	return memcmp( lhsImage->image.resourceName.data, rhsImage->image.resourceName.data, lhsImage->image.resourceName.length ) == 0u;
}

ImAppResourceStorage* ImAppResourceStorageCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppRenderer* renderer, ImUiContext* imui )
{
	ImAppResourceStorage* storage = IMUI_MEMORY_NEW_ZERO( allocator, ImAppResourceStorage );

	storage->allocator	= allocator;
	storage->platform	= platform;
	storage->imui		= imui;
	storage->renderer	= renderer;

	if( !ImUiHashMapConstructSize( &storage->imageMap, allocator, sizeof( ImAppResource* ), ImAppStorageMapImageHash, ImAppStorageMapIsKeyEquals, 64u ) )
	{
		ImUiMemoryFree( allocator, storage );
		return NULL;
	}

	return storage;
}

void ImAppResourceStorageDestroy( ImAppResourceStorage* storage )
{
	while( storage->defaultImages.firstResource )
	{
		ImAppResourceStorageFreeImageInternal( storage, storage->defaultImages.firstResource );
	}

	while( storage->managedImages.firstResource )
	{
		ImAppResourceStorageFreeImageInternal( storage, storage->managedImages.firstResource );
	}

	while( storage->unusedImages.firstResource )
	{
		ImAppResourceStorageFreeImageInternal( storage, storage->unusedImages.firstResource );
	}

	ImUiMemoryFree( storage->allocator, storage );
}

void ImAppResourceStorageUpdate( ImAppResourceStorage* storage )
{
	while( storage->unusedImages.firstResource )
	{
		ImAppResourceStorageFreeImageInternal( storage, storage->unusedImages.firstResource );
	}

	storage->unusedImages = storage->managedImages;
	memset( &storage->managedImages, 0, sizeof( storage->managedImages ) );

	for( ImAppResource* resource = storage->unusedImages.firstResource; resource != NULL; resource = resource->nextResource )
	{
		resource->state = ImAppImageState_Unused;
	}
}

bool ImAppResourceStorageRecreateEverything( ImAppResourceStorage* storage )
{
	return false;
}

ImAppImage* ImAppResourceStorageImageFindOrLoad( ImAppResourceStorage* storage, ImUiStringView resourceName, bool autoFree )
{
	bool isNew;
	ImAppResource** mapImage = (ImAppResource**)ImUiHashMapInsertNew( &storage->imageMap, &resourceName, &isNew );
	if( !mapImage )
	{
		return NULL;
	}

	if( !isNew )
	{
		ImAppResource* resource = *mapImage;
		if( resource->state != ImAppImageState_Default )
		{
			ImAppResourceStorageChangeState( storage, resource, autoFree ? ImAppImageState_Managed : ImAppImageState_Default );
		}

		if( resource->state == ImAppImageState_Unused )
		{
			ImAppResourceListRemove( &storage->unusedImages, resource );
			ImAppResourceListAdd( &storage->managedImages, resource );

			resource->state = ImAppImageState_Managed;
		}

		return &resource->image;
	}

	const ImAppBlob imageBlob = ImAppPlatformResourceLoad( storage->platform, resourceName );
	if( imageBlob.data == NULL )
	{
		ImUiHashMapRemove( &storage->imageMap, &resourceName );
		return NULL;
	}

	uint8_t* pixelData;
	unsigned width;
	unsigned height;
	const unsigned pngResult = lodepng_decode_memory( &pixelData, &width, &height, (const unsigned char*)imageBlob.data, imageBlob.size, LCT_RGBA, 8u );
	ImUiMemoryFree( storage->allocator, imageBlob.data );

	if( pngResult != 0 )
	{
		ImUiHashMapRemove( &storage->imageMap, &resourceName );
		return NULL;
	}

	ImAppRendererTexture* texture = ImAppRendererTextureCreateFromMemory( storage->renderer, pixelData, width, height, ImAppRendererFormat_RGBA8, ImAppRendererShading_Translucent );
	free( pixelData );

	if( texture == NULL )
	{
		ImUiHashMapRemove( &storage->imageMap, &resourceName );
		return NULL;
	}

	ImAppResource* image = (ImAppResource*)ImUiMemoryAllocZero( storage->allocator, IMUI_OFFSETOF( ImAppResource, resourceName ) + resourceName.length + 1u );
	image->image.resourceName	= ImUiStringViewCreateLength( image->resourceName, resourceName.length );
	image->image.pTexture		= texture;
	image->image.size			= ImUiSizeCreate( (float)width, (float)height );

	memcpy( image->resourceName, resourceName.data, resourceName.length );

	ImAppResourceStorageChangeState( storage, image, autoFree ? ImAppImageState_Managed : ImAppImageState_Default );

	return &image->image;
}

ImAppImage* ImAppResourceStorageImageCreateFromMemory( ImAppResourceStorage* storage, const void* pixelData, int width, int height )
{
	ImAppRendererTexture* texture = ImAppRendererTextureCreateFromMemory( storage->renderer, pixelData, width, height, ImAppRendererFormat_RGBA8, ImAppRendererShading_Translucent );
	if( texture == NULL )
	{
		return NULL;
	}

	ImAppResource* image = IMUI_MEMORY_NEW_ZERO( storage->allocator, ImAppResource );
	image->image.pTexture	= texture;
	image->image.size		= ImUiSizeCreate( (float)width, (float)height );

	ImAppResourceStorageChangeState( storage, image, ImAppImageState_Default );

	return &image->image;
}

ImUiFont* ImAppResourceStorageFontCreate( ImAppResourceStorage* storage, ImUiStringView fontName, float fontSize )
{
	const ImAppBlob fontBlob = ImAppPlatformResourceLoadSystemFont( storage->platform, fontName );
	if( fontBlob.data == NULL )
	{
		return NULL;
	}

	ImUiFontTrueTypeData* ttf = ImUiFontTrueTypeDataCreate( storage->imui, fontBlob.data, fontBlob.size );
	if( !ttf )
	{
		ImUiMemoryFree( storage->allocator, fontBlob.data );
		return false;
	}

	ImUiFontTrueTypeDataAddCodepointRange( ttf, 0x20, 0x7e );
	ImUiFontTrueTypeDataAddCodepointRange( ttf, 0x370, 0x3ff );
	ImUiFontTrueTypeDataAddCodepointRange( ttf, 0xfffd, 0xfffd );

	uint32_t width;
	uint32_t height;
	ImUiFontTrueTypeDataCalculateMinTextureSize( ttf, fontSize, &width, &height );
	width = (width + 4u - 1u) & (0 - 4);
	height = (height + 4u - 1u) & (0 - 4);

	void* textureData = malloc( width * height );
	if( !textureData )
	{
		ImUiFontTrueTypeDataDestroy( ttf );
		ImUiMemoryFree( storage->allocator, fontBlob.data );
		return false;
	}

	ImUiFontTrueTypeImage* image = ImUiFontTrueTypeDataGenerateTextureData( ttf, fontSize, textureData, width * height, width, height );
	if( !image )
	{
		ImUiFontTrueTypeDataDestroy( ttf );
		ImUiMemoryFree( storage->allocator, fontBlob.data );
		return false;
	}

	ImAppRendererTexture* texture = ImAppRendererTextureCreateFromMemory( storage->renderer, textureData, width, height, ImAppRendererFormat_R8, ImAppRendererShading_Font );
	free( textureData );

	if( !texture )
	{
		ImUiFontTrueTypeDataDestroy( ttf );
		ImUiMemoryFree( storage->allocator, fontBlob.data );
		return false;
	}

	ImUiTexture uiTexture;
	uiTexture.data = texture;
	uiTexture.size = ImUiSizeCreate( (float)width, (float)height );

	ImUiFont* font = ImUiFontCreateTrueType( storage->imui, image, uiTexture );

	ImUiFontTrueTypeDataDestroy( ttf );
	ImUiMemoryFree( storage->allocator, fontBlob.data );

	return font;
}


void ImAppResourceStorageImageFree( ImAppResourceStorage* storage, ImAppImage* image )
{
	ImAppResource* pInternalImage = (ImAppResource*)image;
	ImAppResourceStorageFreeImageInternal( storage, pInternalImage );
}

static void ImAppResourceStorageFreeImageInternal( ImAppResourceStorage* storage, ImAppResource* image )
{
	ImAppResourceStorageChangeState( storage, image, ImAppImageState_Invalid );

	ImUiHashMapRemove( &storage->imageMap, &image );

	ImAppRendererTextureDestroy( storage->renderer, image->image.pTexture );
	image->image.pTexture = NULL;

	ImUiMemoryFree( storage->allocator, image );
}

static void ImAppResourceStorageChangeState( ImAppResourceStorage* storage, ImAppResource* resource, ImAppResourceState state )
{
	if( resource->state == state )
	{
		return;
	}

	ImAppResourceList* pOldList = NULL;
	switch( resource->state )
	{
	case ImAppImageState_Invalid:
		break;

	case ImAppImageState_Default:
		pOldList = &storage->defaultImages;
		break;

	case ImAppImageState_Managed:
		pOldList = &storage->managedImages;
		break;

	case ImAppImageState_Unused:
		pOldList = &storage->unusedImages;
		break;

	default:
		break;
	}

	if( pOldList != NULL )
	{
		ImAppResourceListRemove( pOldList, resource );
	}

	ImAppResourceList* pNewList = NULL;
	switch( state )
	{
	case ImAppImageState_Invalid:
		break;

	case ImAppImageState_Default:
		pNewList = &storage->defaultImages;
		break;

	case ImAppImageState_Managed:
		pNewList = &storage->managedImages;
		break;

	case ImAppImageState_Unused:
		pNewList = &storage->unusedImages;
		break;

	default:
		break;
	}

	if( pNewList != NULL )
	{
		ImAppResourceListAdd( pNewList, resource );
	}

	resource->state = state;
}

static void ImAppResourceListAdd( ImAppResourceList* list, ImAppResource* resource )
{
	IMAPP_ASSERT( resource->prevResource == NULL );
	IMAPP_ASSERT( resource->nextResource == NULL );

	if( list->firstResource == NULL )
	{
		list->firstResource = resource;
	}

	if( list->lastResource == NULL )
	{
		list->lastResource = resource;
		return;
	}

	IMAPP_ASSERT( list->lastResource->nextResource == NULL );
	list->lastResource->nextResource = resource;
	resource->prevResource = list->lastResource;

	list->lastResource = resource;
}

static void ImAppResourceListRemove( ImAppResourceList* list, ImAppResource* resource )
{
	if( list->firstResource == resource )
	{
		list->firstResource = resource->nextResource;
	}

	if( list->lastResource == resource )
	{
		list->lastResource = resource->prevResource;
	}

	if( resource->prevResource != NULL )
	{
		resource->prevResource->nextResource = resource->nextResource;
	}

	if( resource->nextResource != NULL )
	{
		resource->nextResource->prevResource = resource->prevResource;
	}

	resource->prevResource = NULL;
	resource->nextResource = NULL;
}

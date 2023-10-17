#include "imapp_res_sys.h"

#include "imapp_debug.h"
#include "imapp_internal.h"
#include "imapp_platform.h"
#include "imapp_renderer.h"

#include "imapp_res_sys_internal.h"

#include <spng/spng.h>

#include <stdlib.h>
#include <string.h>

struct ImAppResSys
{
	ImUiAllocator*		allocator;
	ImAppPlatform*		platform;
	ImAppRenderer*		renderer;
	ImUiContext*		imui;

	ImUiHashMap			resMap;
	ImUiHashMap			nameMap;
	ImAppRes*			firstUnusedRes;

	ImAppThread*		thread;
	ImAppAtomic32		runThread;

	ImAppResEventQueue	sendQueue;
	ImAppResEventQueue	receiveQueue;
};

static bool		ImAppResEventQueueConstruct( ImAppResSys* ressys, ImAppResEventQueue* queue, uintsize initSize );
static void		ImAppResEventQueueDestruct( ImAppResSys* ressys, ImAppResEventQueue* queue );
static bool		ImAppResEventQueuePush( ImAppResSys* ressys, ImAppResEventQueue* queue, const ImAppResEvent* resEvent );
static bool		ImAppResEventQueuePop( ImAppResEventQueue* queue, ImAppResEvent* outEvent, bool wait );

static ImUiHash	ImAppResSysResMapHash( const void* key );
static bool		ImAppResSysResMapIsKeyEquals( const void* lhs, const void* rhs );
static ImUiHash	ImAppResSysNameMapHash( const void* key );
static bool		ImAppResSysNameMapIsKeyEquals( const void* lhs, const void* rhs );

static void		ImAppResSysThreadEntry( void* arg );

static void		ImAppResSysChangeState( ImAppResSys* ressys, ImAppRes* res, ImAppResState state );
static void		ImAppResSysDecRefCount( ImAppResSys* ressys, ImAppRes* res );
static void		ImAppResSysFree( ImAppResSys* ressys, ImAppRes* res );

static void		ImAppResListAdd( ImAppRes** list, ImAppRes* res );
static void		ImAppResListRemove( ImAppRes** list, ImAppRes* res );

static void		ImAppResSysImageLoad( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void		ImAppResSysDecodePng( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void		ImAppResSysDecodeJpeg( ImAppResSys* ressys, ImAppResEvent* resEvent );

ImAppResSys* ImAppResSysCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppRenderer* renderer, ImUiContext* imui )
{
	ImAppResSys* ressys = IMUI_MEMORY_NEW_ZERO( allocator, ImAppResSys );

	ressys->allocator	= allocator;
	ressys->platform	= platform;
	ressys->imui		= imui;
	ressys->renderer	= renderer;

	if( !ImUiHashMapConstructSize( &ressys->resMap, allocator, sizeof( ImAppRes* ), ImAppResSysResMapHash, ImAppResSysResMapIsKeyEquals, 64u ) ||
		!ImUiHashMapConstructSize( &ressys->nameMap, allocator, sizeof( ImAppRes* ), ImAppResSysNameMapHash, ImAppResSysNameMapIsKeyEquals, 64u ) ||
		!ImAppResEventQueueConstruct( ressys, &ressys->sendQueue, 16u ) ||
		!ImAppResEventQueueConstruct( ressys, &ressys->receiveQueue, 16u ) )
	{
		ImAppResSysDestroy( ressys );
		return NULL;
	}

	ImAppPlatformAtomicSet( &ressys->runThread, 1u );
	ressys->thread = ImAppPlatformThreadCreate( platform, "res sys", ImAppResSysThreadEntry, ressys );

	return ressys;
}

void ImAppResSysDestroy( ImAppResSys* ressys )
{
	if( ressys->thread )
	{
		ImAppPlatformAtomicSet( &ressys->runThread, 0u );
		ImAppPlatformThreadDestroy( ressys->thread );
		ressys->thread = NULL;
	}

	ImAppResEventQueueDestruct( ressys, &ressys->sendQueue );
	ImAppResEventQueueDestruct( ressys, &ressys->receiveQueue );

	ImUiHashMapDestruct( &ressys->resMap );
	ImUiHashMapDestruct( &ressys->nameMap );

	ImUiMemoryFree( ressys->allocator, ressys );
}

void ImAppResSysUpdate( ImAppResSys* ressys )
{

}

bool ImAppResSysRecreateEverything( ImAppResSys* ressys )
{
	return false;
}

ImAppResPak*				ImAppResSysOpen( ImAppResSys* ressys, const char* resourceName );
void						ImAppResSysClose( ImAppResSys* ressys, ImAppResPak* respak );

ImUiTexture					ImAppResPakGetImage( ImAppResPak* pak, const char* name );
ImUiTexture					ImAppResPakGetImageIndex( ImAppResPak* pak, uint16_t index );
ImUiSkin					ImAppResPakGetSkin( ImAppResPak* pak, const char* name );
ImUiSkin					ImAppResPakGetSkinIndex( ImAppResPak* pak, uint16_t index );
ImUiFont*					ImAppResPakGetFont( ImAppResPak* pak, const char* name );
ImUiFont*					ImAppResPakGetFontIndex( ImAppResPak* pak, uint16_t index );
const ImUiToolboxConfig*	ImAppResPakGetTheme( ImAppResPak* pak, const char* name );
const ImUiToolboxConfig*	ImAppResPakGetThemeIndex( ImAppResPak* pak, uint16_t index );
ImAppBlob					ImAppResPakGetBlob( ImAppResPak* pak, const char* name );
ImAppBlob					ImAppResPakGetBlobIndex( ImAppResPak* pak, uint16_t index );

void ImAppResPakActivateTheme( ImAppResPak* pak, const char* name )
{

}

ImAppImage* ImAppResSysImageCreateResource( ImAppResSys* ressys, const char* resourceName )
{
	const uintsize resourceNameLength = strlen( resourceName );

	ImAppImage* image = (ImAppImage*)ImUiMemoryAlloc( ressys->allocator, sizeof( *image ) + resourceNameLength );
	memcpy( image->resourceName, resourceName, resourceNameLength );
	image->resourceName[ resourceNameLength ] = '\0';

	image->texture = NULL;

	ImAppResEvent loadEvent;
	loadEvent.type				= ImAppResEventType_LoadImage;
	loadEvent.data.image.image	= image;

	if( !ImAppResEventQueuePush( ressys, &ressys->sendQueue, &loadEvent ) )
	{
		ImUiMemoryFree( ressys->allocator, image );
		return NULL;
	}

	return image;
}

ImAppImage* ImAppResSysImageCreateRaw( ImAppResSys* ressys, const void* pixelData, int width, int height )
{
	ImAppImage* image = IMUI_MEMORY_NEW( ressys->allocator, ImAppImage );
	image->texture				= ImAppRendererTextureCreateFromMemory( ressys->renderer, pixelData, width, height, ImAppRendererFormat_RGBA8, ImAppRendererShading_Translucent );
	image->width				= width;
	image->height				= height;
	image->resourceName[ 0u ]	= '\0';

	if( !image->texture )
	{
		ImUiMemoryFree( ressys->allocator, image );
	}

	return image;
}

ImAppImage* ImAppResSysImageCreatePng( ImAppResSys* ressys, const void* imageData, size_t imageDataSize )
{
	ImAppImage* image = IMUI_MEMORY_NEW( ressys->allocator, ImAppImage );
	image->texture				= NULL;
	image->resourceName[ 0u ]	= '\0';

	ImAppResEvent decodeEvent;
	decodeEvent.type				= ImAppResEventType_DecodePng;
	decodeEvent.data.image.image	= image;

	if( !ImAppResEventQueuePush( ressys, &ressys->sendQueue, &decodeEvent ) )
	{
		ImUiMemoryFree( ressys->allocator, image );
		return NULL;
	}

	return image;
}

bool ImAppResSysImageIsLoaded( ImAppResSys* ressys, ImAppImage* image )
{
	return image->texture != NULL;
}

void ImAppResSysImageFree( ImAppResSys* ressys, ImAppImage* image )
{
	if( image->texture )
	{
		ImAppRendererTextureDestroy( ressys->renderer, image->texture );
	}

	ImUiMemoryFree( ressys->allocator, image );
}

ImUiFont* ImAppResSysFontCreateSystem( ImAppResSys* ressys, const char* fontName, float fontSize )
{
	const ImAppBlob fontBlob = ImAppPlatformResourceLoadSystemFont( ressys->platform, fontName );
	if( fontBlob.data == NULL )
	{
		return NULL;
	}

	ImUiFontTrueTypeData* ttf = ImUiFontTrueTypeDataCreate( ressys->imui, fontBlob.data, fontBlob.size );
	if( !ttf )
	{
		ImUiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	ImUiFontTrueTypeDataAddCodepointRange( ttf, 0x20, 0x7e );
	ImUiFontTrueTypeDataAddCodepointRange( ttf, 0xa0, 0xff );
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
		ImUiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	ImUiFontTrueTypeImage* image = ImUiFontTrueTypeDataGenerateTextureData( ttf, fontSize, textureData, width * height, width, height );
	if( !image )
	{
		ImUiFontTrueTypeDataDestroy( ttf );
		ImUiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	ImAppRendererTexture* texture = ImAppRendererTextureCreateFromMemory( ressys->renderer, textureData, width, height, ImAppRendererFormat_R8, ImAppRendererShading_Font );
	free( textureData );

	if( !texture )
	{
		ImUiFontTrueTypeDataDestroy( ttf );
		ImUiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	ImUiTexture uiTexture;
	uiTexture.data		= texture;
	uiTexture.width		= width;
	uiTexture.height	= height;

	ImUiFont* font = ImUiFontCreateTrueType( ressys->imui, image, uiTexture );

	ImUiFontTrueTypeDataDestroy( ttf );
	ImUiMemoryFree( ressys->allocator, fontBlob.data );

	return font;
}

static bool ImAppResEventQueueConstruct( ImAppResSys* ressys, ImAppResEventQueue* queue, uintsize initSize )
{
	queue->semaphore		= ImAppPlatformSemaphoreCreate( ressys->platform );
	queue->mutex			= ImAppPlatformMutexCreate( ressys->platform );
	queue->events			= IMUI_MEMORY_ARRAY_NEW( ressys->allocator, ImAppResEvent, initSize );
	queue->eventCount		= 0u;
	queue->eventCapacity	= initSize;

	if( !queue->semaphore ||
		!queue->mutex ||
		!queue->events )
	{
		ImAppResEventQueueDestruct( ressys, queue );
		return false;
	}

	return true;
}

static void ImAppResEventQueueDestruct( ImAppResSys* ressys, ImAppResEventQueue* queue )
{
	ImUiMemoryFree( ressys->allocator, queue->events );
	queue->events			= NULL;
	queue->eventCount		= 0u;
	queue->eventCapacity	= 0u;

	if( queue->mutex )
	{
		ImAppPlatformMutexDestroy( ressys->platform, queue->mutex );
		queue->mutex = NULL;
	}

	if( queue->semaphore )
	{
		ImAppPlatformSemaphoreDestroy( ressys->platform, queue->semaphore );
		queue->semaphore = NULL;
	}
}

static bool ImAppResEventQueuePush( ImAppResSys* ressys, ImAppResEventQueue* queue, const ImAppResEvent* resEvent )
{
	ImAppPlatformMutexLock( queue->mutex );

	if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY( ressys->allocator, queue->events, queue->eventCapacity, queue->eventCount + 1u ) )
	{
		ImAppPlatformMutexUnlock( queue->mutex );
		return false;
	}

	queue->events[ queue->eventCount ] = *resEvent;
	queue->eventCount++;

	ImAppPlatformMutexUnlock( queue->mutex );

	ImAppPlatformSemaphoreInc( queue->semaphore );
	return true;
}

static bool ImAppResEventQueuePop( ImAppResEventQueue* queue, ImAppResEvent* outEvent, bool wait );

static ImUiHash ImAppResSysResMapHash( const void* key )
{
	const ImAppRes* res = *(const ImAppRes**)key;
	return res->key;
}

static bool ImAppResSysResMapIsKeyEquals( const void* lhs, const void* rhs )
{
	const ImAppRes* lhsRes = *(const ImAppRes**)lhs;
	const ImAppRes* rhsRes = *(const ImAppRes**)rhs;
	return lhsRes->key == rhsRes->key;
}

static ImUiHash ImAppResSysNameMapHash( const void* key )
{
	const ImAppRes* res = *(const ImAppRes**)key;
	return ImUiHashString( res->name, 0u );
}

static bool ImAppResSysNameMapIsKeyEquals( const void* lhs, const void* rhs )
{
	const ImAppRes* lhsRes = *(const ImAppRes**)lhs;
	const ImAppRes* rhsRes = *(const ImAppRes**)rhs;

	if( lhsRes->name.length !=  rhsRes->name.length )
	{
		return false;
	}

	if( lhsRes->name.length == 0u )
	{
		return true;
	}

	if( lhsRes->name.data[ 0u ] != rhsRes->name.data[ 0u ] )
	{
		return false;
	}

	return memcmp( lhsRes->name.data, rhsRes->name.data, lhsRes->name.length ) == 0u;
}

static void ImAppResSysThreadEntry( void* arg )
{
	ImAppResSys* ressys = (ImAppResSys*)arg;

	while( ImAppPlatformAtomicGet( &ressys->runThread ) )
	{

	}
}

//static void ImAppResSysImageLoad( ImAppResSys* ressys, ImAppResEvent* resEvent )
//{
//	const ImAppBlob imageBlob = ImAppPlatformResourceLoad( ressys->platform, resourceName );
//	if( imageBlob.data == NULL )
//	{
//		ImUiHashMapRemove( &ressys->imageMap, &resourceName );
//		return NULL;
//	}
//
//	uint32 width;
//	uint32 height;
//	ImAppRendererTexture* texture = ImAppResSysTextureCreatePng( ressys, imageBlob.data, imageBlob.size, &width, &height );
//	ImUiMemoryFree( ressys->allocator, imageBlob.data );
//	if( texture == NULL )
//	{
//		ImUiHashMapRemove( &ressys->imageMap, &resourceName );
//		return NULL;
//	}
//
//	ImAppResource* image = (ImAppResource*)ImUiMemoryAllocZero( ressys->allocator, IMUI_OFFSETOF( ImAppResource, resourceName ) + resourceNameStr.length + 1u );
//	image->image.resourceName	= ImUiStringViewCreateLength( image->resourceName, resourceNameStr.length );
//	image->image.texture		= texture;
//	image->image.width			= width;
//	image->image.height			= height;
//
//	memcpy( image->resourceName, resourceNameStr.data, resourceNameStr.length );
//
//	ImAppResSysChangeState( ressys, image, autoFree ? ImAppImageState_Managed : ImAppImageState_Default );
//
//	*mapImage = image;
//	return &image->image;
//}
//
//
//
//
//static ImAppRendererTexture* ImAppResSysTextureCreatePng( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize, uint32* width, uint32* height )
//{
//	spng_ctx* spng = spng_ctx_new( 0 );
//	const int bufferResult = spng_set_png_buffer( spng, imageData, imageDataSize );
//	if( bufferResult != SPNG_OK )
//	{
//		IMAPP_DEBUG_LOGE( "Failed to set PNG buffer. Result: %d", bufferResult );
//		spng_ctx_free( spng );
//		return NULL;
//	}
//
//	uintsize pixelDataSize;
//	const int sizeResult = spng_decoded_image_size( spng, SPNG_FMT_RGBA8, &pixelDataSize );
//	if( sizeResult != SPNG_OK )
//	{
//		IMAPP_DEBUG_LOGE( "Failed to calculate PNG size. Result: %d", sizeResult );
//		spng_ctx_free( spng );
//		return NULL;
//	}
//
//	struct spng_ihdr ihdr;
//	const int headerResult = spng_get_ihdr( spng, &ihdr );
//	if( headerResult != SPNG_OK )
//	{
//		IMAPP_DEBUG_LOGE( "Failed to get PNG header. Result: %d", headerResult );
//		spng_ctx_free( spng );
//		return NULL;
//	}
//
//	enum spng_format sourceImageFormat;
//	ImAppRendererFormat targetImageFormat;
//	ImAppRendererShading targetShading;
//	switch( ihdr.color_type )
//	{
//	case SPNG_COLOR_TYPE_GRAYSCALE:
//		sourceImageFormat	= SPNG_FMT_G8;
//		targetImageFormat	= ImAppRendererFormat_R8;
//		targetShading		= ImAppRendererShading_Translucent;
//		break;
//
//	case SPNG_COLOR_TYPE_INDEXED:
//	case SPNG_COLOR_TYPE_TRUECOLOR:
//		sourceImageFormat	= SPNG_FMT_RGB8;
//		targetImageFormat	= ImAppRendererFormat_RGB8;
//		targetShading		= ImAppRendererShading_Opaque;
//		break;
//
//	case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
//		sourceImageFormat	= SPNG_FMT_RGBA8;
//		targetImageFormat	= ImAppRendererFormat_RGBA8;
//		targetShading		= ImAppRendererShading_Translucent;
//		break;
//
//	case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
//	default:
//		{
//			IMAPP_DEBUG_LOGE( "Not supported PNG format. Format: %d", ihdr.color_type );
//			spng_ctx_free( spng );
//			return NULL;
//		}
//		break;
//
//	}
//
//	void* pixelData = ImUiMemoryAlloc( ressys->allocator, pixelDataSize );
//	if( !pixelData )
//	{
//		IMAPP_DEBUG_LOGE( "Failed to allocate PNG pixel data. Size: %d", pixelDataSize );
//		spng_ctx_free( spng );
//		return NULL;
//	}
//
//	const int decodeResult = spng_decode_image( spng, pixelData, pixelDataSize, sourceImageFormat, 0 );
//	spng_ctx_free( spng );
//
//	if( decodeResult != 0 )
//	{
//		IMAPP_DEBUG_LOGE( "Failed to decode PNG. Result: %d", decodeResult );
//		ImUiMemoryFree( ressys->allocator, pixelData );
//		return NULL;
//	}
//
//	ImAppRendererTexture* texture = ImAppRendererTextureCreateFromMemory( ressys->renderer, pixelData, ihdr.width, ihdr.height, targetImageFormat, targetShading );
//	free( pixelData );
//
//	if( texture == NULL )
//	{
//		IMAPP_DEBUG_LOGE( "Failed to create Texture for PNG." );
//		return NULL;
//	}
//
//	*width	= ihdr.width;
//	*height	= ihdr.height;
//
//	return texture;
//}
//
//static void ImAppResSysFreeImageInternal( ImAppResSys* ressys, ImAppResource* image )
//{
//	ImAppResSysChangeState( ressys, image, ImAppImageState_Invalid );
//
//	ImUiHashMapRemove( &ressys->imageMap, &image );
//
//	ImAppRendererTextureDestroy( ressys->renderer, image->image.texture );
//	image->image.texture = NULL;
//
//	ImUiMemoryFree( ressys->allocator, image );
//}
//
//static void ImAppResSysChangeState( ImAppResSys* ressys, ImAppResource* resource, ImAppResourceState state )
//{
//	if( resource->state == state )
//	{
//		return;
//	}
//
//	ImAppResourceList* pOldList = NULL;
//	switch( resource->state )
//	{
//	case ImAppImageState_Invalid:
//		break;
//
//	case ImAppImageState_Default:
//		pOldList = &ressys->defaultImages;
//		break;
//
//	case ImAppImageState_Managed:
//		pOldList = &ressys->managedImages;
//		break;
//
//	case ImAppImageState_Unused:
//		pOldList = &ressys->unusedImages;
//		break;
//
//	default:
//		break;
//	}
//
//	if( pOldList != NULL )
//	{
//		ImAppResourceListRemove( pOldList, resource );
//	}
//
//	ImAppResourceList* pNewList = NULL;
//	switch( state )
//	{
//	case ImAppImageState_Invalid:
//		break;
//
//	case ImAppImageState_Default:
//		pNewList = &ressys->defaultImages;
//		break;
//
//	case ImAppImageState_Managed:
//		pNewList = &ressys->managedImages;
//		break;
//
//	case ImAppImageState_Unused:
//		pNewList = &ressys->unusedImages;
//		break;
//
//	default:
//		break;
//	}
//
//	if( pNewList != NULL )
//	{
//		ImAppResourceListAdd( pNewList, resource );
//	}
//
//	resource->state = state;
//}
//
//static void ImAppResourceListAdd( ImAppResourceList* list, ImAppResource* resource )
//{
//	IMAPP_ASSERT( resource->prevResource == NULL );
//	IMAPP_ASSERT( resource->nextResource == NULL );
//
//	if( list->firstResource == NULL )
//	{
//		list->firstResource = resource;
//	}
//
//	if( list->lastResource == NULL )
//	{
//		list->lastResource = resource;
//		return;
//	}
//
//	IMAPP_ASSERT( list->lastResource->nextResource == NULL );
//	list->lastResource->nextResource = resource;
//	resource->prevResource = list->lastResource;
//
//	list->lastResource = resource;
//}
//
//static void ImAppResListRemove( ImAppResourceList* list, ImAppResource* resource )
//{
//	if( list->firstResource == resource )
//	{
//		list->firstResource = resource->nextResource;
//	}
//
//	if( list->lastResource == resource )
//	{
//		list->lastResource = resource->prevResource;
//	}
//
//	if( resource->prevResource != NULL )
//	{
//		resource->prevResource->nextResource = resource->nextResource;
//	}
//
//	if( resource->nextResource != NULL )
//	{
//		resource->nextResource->prevResource = resource->prevResource;
//	}
//
//	resource->prevResource = NULL;
//	resource->nextResource = NULL;
//}

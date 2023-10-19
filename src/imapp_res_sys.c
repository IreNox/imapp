#include "imapp_res_sys.h"

#include "imapp_debug.h"
#include "imapp_internal.h"
#include "imapp_platform.h"
#include "imapp_renderer.h"

#include "imapp_res_sys_internal.h"

#include <spng/spng.h>

#include <stdlib.h>
#include <string.h>

static const byte s_pngHeader[] = { 0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au };
static const byte s_jpegHeader[] = { 0xffu, 0xd8u, 0xffu, 0xe0u, 0x00u, 0x10u, 0x4au, 0x46u, 0x49u, 0x46u, 0x00u };

struct ImAppResSys
{
	ImUiAllocator*		allocator;
	ImAppPlatform*		platform;
	ImAppRenderer*		renderer;
	ImUiContext*		imui;

	ImAppResPak*		firstResPak;

	ImUiHashMap			nameMap;
	ImAppRes*			firstUnusedRes;

	ImAppThread*		thread;
	ImAppAtomic32		runThread;

	ImAppResEventQueue	sendQueue;
	ImAppResEventQueue	receiveQueue;
};

static void			ImAppResSysHandleOpenResPak( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void			ImAppResSysHandleLoadResData( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void			ImAppResSysHandleImage( ImAppResSys* ressys, ImAppResEvent* resEvent );

static ImAppRes*	ImAppResSysLoad( ImAppResPak* pak, uint16 resIndex );

static void			ImAppResThreadEntry( void* arg );
static void			ImAppResThreadHandleOpenResPak( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void			ImAppResThreadHandleLoadResData( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void			ImAppResThreadHandleImageLoad( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void			ImAppResThreadHandleDecodePng( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void			ImAppResThreadHandleDecodeJpeg( ImAppResSys* ressys, ImAppResEvent* resEvent );

static bool			ImAppResEventQueueConstruct( ImAppResSys* ressys, ImAppResEventQueue* queue, uintsize initSize );
static void			ImAppResEventQueueDestruct( ImAppResSys* ressys, ImAppResEventQueue* queue );
static bool			ImAppResEventQueuePush( ImAppResSys* ressys, ImAppResEventQueue* queue, const ImAppResEvent* resEvent );
static bool			ImAppResEventQueuePop( ImAppResEventQueue* queue, ImAppResEvent* outEvent, bool wait );

static ImUiHash		ImAppResSysNameMapHash( const void* key );
static bool			ImAppResSysNameMapIsKeyEquals( const void* lhs, const void* rhs );


static void			ImAppResSysChangeUsage( ImAppResSys* ressys, ImAppRes* res, ImAppResUsage usage );
static void			ImAppResSysDecRefCount( ImAppResSys* ressys, ImAppRes* res );
static void			ImAppResSysFree( ImAppResSys* ressys, ImAppRes* res );

static void			ImAppResListAdd( ImAppRes** list, ImAppRes* res );
static void			ImAppResListRemove( ImAppRes** list, ImAppRes* res );

ImAppResSys* ImAppResSysCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppRenderer* renderer, ImUiContext* imui )
{
	ImAppResSys* ressys = IMUI_MEMORY_NEW_ZERO( allocator, ImAppResSys );

	ressys->allocator	= allocator;
	ressys->platform	= platform;
	ressys->imui		= imui;
	ressys->renderer	= renderer;

	if( !ImUiHashMapConstructSize( &ressys->nameMap, allocator, sizeof( ImAppRes* ), ImAppResSysNameMapHash, ImAppResSysNameMapIsKeyEquals, 64u ) ||
		!ImAppResEventQueueConstruct( ressys, &ressys->sendQueue, 16u ) ||
		!ImAppResEventQueueConstruct( ressys, &ressys->receiveQueue, 16u ) )
	{
		ImAppResSysDestroy( ressys );
		return NULL;
	}

	ImAppPlatformAtomicSet( &ressys->runThread, 1u );
	ressys->thread = ImAppPlatformThreadCreate( platform, "res sys", ImAppResThreadEntry, ressys );

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

	ImUiHashMapDestruct( &ressys->nameMap );

	ImUiMemoryFree( ressys->allocator, ressys );
}

void ImAppResSysUpdate( ImAppResSys* ressys )
{
	{
		ImAppResEvent resEvent;
		while( ImAppResEventQueuePop( &ressys->receiveQueue, &resEvent, false ) )
		{
			switch( resEvent.type )
			{
			case ImAppResEventType_OpenResPak:
				ImAppResSysHandleOpenResPak( ressys, &resEvent );
				break;

			case ImAppResEventType_LoadResData:
				ImAppResSysHandleLoadResData( ressys, &resEvent );
				break;

			case ImAppResEventType_LoadImage:
			case ImAppResEventType_DecodePng:
			case ImAppResEventType_DecodeJpeg:
				ImAppResSysHandleImage( ressys, &resEvent );
				break;
			}
		}
	}
}

static void ImAppResSysHandleOpenResPak( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	ImAppResPak* pak = resEvent->data.pak.pak;

	if( !resEvent->success )
	{
		pak->state = ImAppResState_Error;
		return;
	}

	for( uintsize i = 0; i < pak->resourceCount; ++i )
	{
		ImUiHashMapInsert( &ressys->nameMap, &pak->resources[ i ] );
	}

	pak->state = ImAppResState_Ready;
}

static void ImAppResSysHandleLoadResData( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	ImAppRes* res = resEvent->data.res.res;

	switch( res->key.type )
	{
	case ImAppResPakType_Texture:
		{
			const ImAppResPakTextureHeader* header	= (const ImAppResPakTextureHeader*)resEvent->result.loadRes.data.data;
			const void* pixelData					= header + 1u;

			ImAppRendererFormat format = ImAppRendererFormat_RGBA8;
			switch( header->format )
			{
			case ImAppResPakTextureFormat_PNG24:
			case ImAppResPakTextureFormat_JPEG:
			case ImAppResPakTextureFormat_RGB8:		format = ImAppRendererFormat_RGB8; break;
			case ImAppResPakTextureFormat_PNG32:
			case ImAppResPakTextureFormat_RGBA8:	format = ImAppRendererFormat_RGBA8; break;
			}

			res->data.texture.texture.data		= ImAppRendererTextureCreateFromMemory( ressys->renderer, pixelData, header->width, header->height, format, ImAppRendererShading_Translucent );
			res->data.texture.texture.width		= header->width;
			res->data.texture.texture.height	= header->height;
		}
		break;

	case ImAppResPakType_Font:
		// TODO
		break;

	case ImAppResPakType_Blob:
		res->data.blob.blob = resEvent->result.loadRes.data;
		break;

	case ImAppResPakType_Image:
	case ImAppResPakType_Skin:
	case ImAppResPakType_Theme:
		// no data
		break;
	}

	ImAppResSysChangeUsage( ressys, res, ImAppResUsage_Used );
}

static void ImAppResSysHandleImage( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	ImAppImage* image = resEvent->data.image.image;

	if( !resEvent->success )
	{
		image->state = ImAppResState_Error;
		return;
	}

	const ImAppResEventResultImageData* result = &resEvent->result.image;
	image->width	= result->width;
	image->height	= result->height;
	image->texture	= ImAppRendererTextureCreateFromMemory( ressys->renderer, result->data.data, result->width, result->height, ImAppRendererFormat_RGBA8, ImAppRendererShading_Translucent );

	image->state = ImAppResState_Ready;
}

static ImAppRes* ImAppResSysLoad( ImAppResPak* pak, uint16 resIndex )
{
	IMAPP_ASSERT( resIndex < pak->resourceCount );

	ImAppRes* res = &pak->resources[ resIndex ];
	if( res->state == ImAppResState_Ready )
	{
		res->refCount++;
		ImAppResSysChangeUsage( pak->ressys, res, ImAppResUsage_Used );
		return res;
	}

	switch( res->key.type )
	{
	case ImAppResPakType_Texture:
		{
			ImAppResEvent resEvent;
			resEvent.type			= ImAppResEventType_LoadResData;
			resEvent.data.res.res	= res;

			if( !ImAppResEventQueuePush( pak->ressys, &pak->ressys->sendQueue, &resEvent ) )
			{
				res->state = ImAppResState_Error;
				return NULL;
			}
		}
		break;

	case ImAppResPakType_Image:
		{
			const ImAppResPakResource* sourceRes = ImAppResPakResourceGet( pak->metadata, res->key.index );
			IMAPP_ASSERT( sourceRes );

			ImAppRes* textureRes = ImAppResSysLoad( pak, sourceRes->textureIndex );
			if( !textureRes || textureRes->state != ImAppResState_Ready )
			{
				return NULL;
			}

			const ImAppResPakImageHeader* header = (const ImAppResPakImageHeader*)ImAppResPakResourceGetHeader( pak->metadata, sourceRes );

			res->data.image.textureRes		= textureRes;
			res->data.image.image.texture	= textureRes->data.texture.texture;
			res->data.image.image.uv.u0		= (float)header->x / textureRes->data.texture.texture.width;
			res->data.image.image.uv.v0		= (float)header->y / textureRes->data.texture.texture.height;
			res->data.image.image.uv.u1		= (float)(header->x + header->width) / textureRes->data.texture.texture.width;
			res->data.image.image.uv.v1		= (float)(header->y + header->height) / textureRes->data.texture.texture.height;
		}
		break;

	case ImAppResPakType_Skin:
		{
			const ImAppResPakResource* sourceRes = ImAppResPakResourceGet( pak->metadata, res->key.index );
			IMAPP_ASSERT( sourceRes );

			ImAppRes* textureRes = ImAppResSysLoad( pak, sourceRes->textureIndex );
			if( !textureRes || textureRes->state != ImAppResState_Ready )
			{
				return NULL;
			}

			const ImAppResPakSkinHeader* header = (const ImAppResPakSkinHeader*)ImAppResPakResourceGetHeader( pak->metadata, sourceRes );

			res->data.skin.textureRes	= textureRes;
			res->data.skin.skin.texture	= textureRes->data.texture.texture;
			res->data.skin.skin.uv.u0	= (float)header->x / textureRes->data.texture.texture.width;
			res->data.skin.skin.uv.v0	= (float)header->y / textureRes->data.texture.texture.height;
			res->data.skin.skin.uv.u1	= (float)(header->x + header->width) / textureRes->data.texture.texture.width;
			res->data.skin.skin.uv.v1	= (float)(header->y + header->height) / textureRes->data.texture.texture.height;
			res->data.skin.skin.border	= ImUiBorderCreate( header->top, header->left, header->bottom, header->right );
		}
		break;

	case ImAppResPakType_Font:
		break;

	case ImAppResPakType_Theme:
		{

		}
		break;

	case ImAppResPakType_Blob:

		break;
	}

	res->refCount++;
	ImAppResSysChangeUsage( pak->ressys, res, ImAppResUsage_Used );
	return res;
}

bool ImAppResSysRecreateEverything( ImAppResSys* ressys )
{
	return false;
}

ImAppResPak* ImAppResSysOpen( ImAppResSys* ressys, const char* resourceName )
{
	const uintsize nameLength = strlen( resourceName );

	ImAppResPak* pak = (ImAppResPak*)ImUiMemoryAllocZero( ressys->allocator, sizeof( ImAppResPak ) + nameLength );
	pak->ressys	= ressys;
	memcpy( pak->resourceName, resourceName, nameLength );

	ImAppResEvent resEvent;
	resEvent.type			= ImAppResEventType_OpenResPak;
	resEvent.data.pak.pak	= pak;

	if( !ImAppResEventQueuePush( ressys, &ressys->sendQueue, &resEvent ) )
	{
		ImUiMemoryFree( ressys->allocator, pak );
		return NULL;
	}

	pak->nextResPak = ressys->firstResPak;
	if( pak->nextResPak )
	{
		pak->nextResPak->prevResPak = pak;
	}
	ressys->firstResPak = pak;

	return pak;
}

void ImAppResSysClose( ImAppResSys* ressys, ImAppResPak* pak )
{

}

ImAppResState ImAppResPakGetState( const ImAppResPak* pak )
{
	return pak->state;
}

bool ImAppResPakPreloadResourceIndex( ImAppResPak* pak, uint16_t resIndex )
{
	if( resIndex >= pak->resourceCount )
	{
		return false;
	}

	const ImAppRes* res = ImAppResSysLoad( pak, resIndex );
	if( !res )
	{
		return false;
	}

	return res->state == ImAppResState_Ready;
}

bool ImAppResPakPreloadResourceName( ImAppResPak* pak, ImAppResPakType type, const char* name )
{
	const uint16 resIndex = ImAppResPakFindResourceIndex( pak, type, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return false;
	}

	return ImAppResPakPreloadResourceIndex( pak, resIndex );
}

uint16_t ImAppResPakFindResourceIndex( const ImAppResPak* pak, ImAppResPakType type, const char* name )
{
	ImAppResKey key;
	key.name	= ImUiStringViewCreate( name );
	key.type	= type;
	key.pak		= (ImAppResPak*)pak;

	const ImAppRes** resKey = (const ImAppRes**)ImUiHashMapFind( &pak->ressys->nameMap, &key );
	if( resKey )
	{
		return (*resKey)->key.index;
	}

	return IMAPP_RES_PAK_INVALID_INDEX;
}

ImUiTexture					ImAppResPakGetImage( ImAppResPak* pak, const char* name );
ImUiTexture					ImAppResPakGetImageIndex( ImAppResPak* pak, uint16_t resIndex );
ImUiSkin					ImAppResPakGetSkin( ImAppResPak* pak, const char* name );
ImUiSkin					ImAppResPakGetSkinIndex( ImAppResPak* pak, uint16_t resIndex );
ImUiFont*					ImAppResPakGetFont( ImAppResPak* pak, const char* name );
ImUiFont*					ImAppResPakGetFontIndex( ImAppResPak* pak, uint16_t resIndex );
const ImUiToolboxConfig*	ImAppResPakGetTheme( ImAppResPak* pak, const char* name );
const ImUiToolboxConfig*	ImAppResPakGetThemeIndex( ImAppResPak* pak, uint16_t resIndex );
ImAppBlob					ImAppResPakGetBlob( ImAppResPak* pak, const char* name );
ImAppBlob					ImAppResPakGetBlobIndex( ImAppResPak* pak, uint16_t resIndex );

void ImAppResPakActivateTheme( ImAppResPak* pak, const char* name )
{

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

ImAppImage* ImAppResSysImageCreatePng( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize )
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

ImAppImage* ImAppResSysImageCreateJpeg( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize )
{
	ImAppImage* image = IMUI_MEMORY_NEW( ressys->allocator, ImAppImage );
	image->texture				= NULL;
	image->resourceName[ 0u ]	= '\0';

	ImAppResEvent decodeEvent;
	decodeEvent.type				= ImAppResEventType_DecodeJpeg;
	decodeEvent.data.image.image	= image;

	if( !ImAppResEventQueuePush( ressys, &ressys->sendQueue, &decodeEvent ) )
	{
		ImUiMemoryFree( ressys->allocator, image );
		return NULL;
	}

	return image;
}

ImAppResState ImAppResSysImageGetState( ImAppResSys* ressys, ImAppImage* image )
{
	return image->state;
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

static void ImAppResThreadEntry( void* arg )
{
	ImAppResSys* ressys = (ImAppResSys*)arg;

	ImAppResEvent resEvent;
	while( ImAppPlatformAtomicGet( &ressys->runThread ) )
	{
		if( !ImAppResEventQueuePop( &ressys->sendQueue, &resEvent, true ) )
		{
			continue;
		}

		resEvent.success = false;

		switch( resEvent.type )
		{
		case ImAppResEventType_OpenResPak:
			ImAppResThreadHandleOpenResPak( ressys, &resEvent );
			break;

		case ImAppResEventType_LoadResData:
			ImAppResThreadHandleLoadResData( ressys, &resEvent );
			break;

		case ImAppResEventType_LoadImage:
			ImAppResThreadHandleImageLoad( ressys, &resEvent );
			break;

		case ImAppResEventType_DecodePng:
			ImAppResThreadHandleDecodePng( ressys, &resEvent );
			break;

		case ImAppResEventType_DecodeJpeg:
			ImAppResThreadHandleDecodeJpeg( ressys, &resEvent );
			break;
		}

		ImAppResEventQueuePush( ressys, &ressys->receiveQueue, &resEvent );
	}
}

static void ImAppResThreadHandleOpenResPak( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	ImAppResPak* pak = resEvent->data.pak.pak;

	ImAppFile* file = ImAppPlatformResourceOpen( ressys->platform, pak->resourceName );
	if( !file )
	{
		IMAPP_DEBUG_LOGE( "Failed to open ResPak." );
		return;
	}

	ImAppResPakHeader header;
	if( ImAppPlatformResourceRead( file, &header, sizeof( header ), 0u ) != sizeof( header ) )
	{
		IMAPP_DEBUG_LOGE( "Failed to read ResPak header." );
		ImAppPlatformResourceClose( ressys->platform, file );
		return;
	}

	if( memcmp( header.magic, IMAPP_RES_PAK_MAGIC, sizeof( header.magic ) ) != 0 )
	{
		char magicBuffer[] = { header.magic[ 0u ], header.magic[ 1u ],header.magic[ 2u ],header.magic[ 3u ], '\0' };
		IMAPP_DEBUG_LOGE( "Invalid ResPak file. Magic doesn't match. Got: '%s', Expected: '%s'", magicBuffer, IMAPP_RES_PAK_MAGIC );
		return;
	}

	pak->metadata		= (byte*)ImUiMemoryAlloc( ressys->allocator, header.resourcesOffset );
	pak->metadataSize	= header.resourcesOffset;

	if( !pak->metadata )
	{
		IMAPP_DEBUG_LOGE( "Failed to allocate ResPak metadata." );
		return;
	}

	const uintsize metadataReadResult = ImAppPlatformResourceRead( file, pak->metadata, pak->metadataSize, 0u );
	ImAppPlatformResourceClose( ressys->platform, file );

	if( metadataReadResult != pak->metadataSize )
	{
		IMAPP_DEBUG_LOGE( "Failed to read ResPak metadata." );
		return;
	}

	pak->resources		= IMUI_MEMORY_ARRAY_NEW_ZERO( ressys->allocator, ImAppRes, header.resourceCount );
	pak->resourceCount	= header.resourceCount;

	if( !pak->resources )
	{
		IMAPP_DEBUG_LOGE( "Failed to allocate ResPak resources." );
		return;
	}

	const ImAppResPakResource* sourceResources = (const ImAppResPakResource*)(pak->metadata + sizeof( ImAppResPakHeader ));
	for( uintsize i = 0; i < pak->resourceCount; ++i )
	{
		const ImAppResPakResource* sourceRes = &sourceResources[ i ];
		ImAppRes* targetRes = &pak->resources[ i ];

		targetRes->key.index	= (uint16)i;
		targetRes->key.name		= ImAppResPakResourceGetName( pak->metadata, sourceResources );
		targetRes->key.type		= (ImAppResPakType)sourceRes->type;
		targetRes->key.pak		= pak;
	}

	resEvent->success = true;
}

static void ImAppResThreadHandleLoadResData( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	ImAppRes* res = resEvent->data.res.res;
	ImAppResPak* pak = res->key.pak;

	const ImAppResPakResource* sourceRes = ImAppResPakResourceGet( pak->metadata, res->key.index );
	if( !sourceRes )
	{
		IMAPP_DEBUG_LOGE( "Could not find resource %d in pak '%s'.", res->key.index, pak->resourceName );
		return;
	}

	const ImAppBlob resData = ImAppPlatformResourceLoadRange( ressys->platform, pak->resourceName, sourceRes->dataOffset, sourceRes->dataSize );
	if( !resData.data )
	{
		const ImUiStringView resName = ImAppResPakResourceGetName( pak->metadata, sourceRes );
		IMAPP_DEBUG_LOGE( "Failed to load data of resource '%s' in pak '%s'.", resName.data, pak->resourceName );
		return;
	}

	resEvent->result.loadRes.data = resData;
	resEvent->success = true;
}

static void ImAppResThreadHandleImageLoad( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	ImAppImage* image = resEvent->data.image.image;

	const ImAppBlob data = ImAppPlatformResourceLoad( ressys->platform, image->resourceName );
	if( !data.data )
	{
		IMAPP_DEBUG_LOGE( "Failed to load image '%s'.", image->resourceName );
		return;
	}

	const bool isPng = data.size > sizeof( s_pngHeader ) && memcmp( data.data, s_pngHeader, sizeof( s_pngHeader ) ) == 0;
	const bool isJpeg = data.size > sizeof( s_jpegHeader ) && memcmp( data.data, s_jpegHeader, sizeof( s_jpegHeader ) ) == 0;
	if( isPng || isJpeg )
	{
		ImAppResEvent decodeEvent;
		decodeEvent.type					= isPng ? ImAppResEventType_DecodePng : ImAppResEventType_DecodeJpeg;
		decodeEvent.data.decode.image		= image;
		decodeEvent.data.decode.sourceData	= data;
		decodeEvent.success					= false;

		if( isPng )
		{
			ImAppResThreadHandleDecodePng( ressys, &decodeEvent );
		}
		else
		{
			ImAppResThreadHandleDecodeJpeg( ressys, &decodeEvent );
		}

		if( !decodeEvent.success )
		{
			ImAppPlatformResourceFree( ressys->platform, data );
			return;
		}

		resEvent->result.image = decodeEvent.result.image;
	}

	resEvent->success = true;
}

static void ImAppResThreadHandleDecodePng( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	spng_ctx* spng = spng_ctx_new( 0 );
	const int bufferResult = spng_set_png_buffer( spng, resEvent->data.decode.sourceData.data, resEvent->data.decode.sourceData.size );
	if( bufferResult != SPNG_OK )
	{
		IMAPP_DEBUG_LOGE( "Failed to set PNG buffer. Result: %d", bufferResult );
		spng_ctx_free( spng );
		return;
	}

	uintsize pixelDataSize;
	const int sizeResult = spng_decoded_image_size( spng, SPNG_FMT_RGBA8, &pixelDataSize );
	if( sizeResult != SPNG_OK )
	{
		IMAPP_DEBUG_LOGE( "Failed to calculate PNG size. Result: %d", sizeResult );
		spng_ctx_free( spng );
		return;
	}

	struct spng_ihdr ihdr;
	const int headerResult = spng_get_ihdr( spng, &ihdr );
	if( headerResult != SPNG_OK )
	{
		IMAPP_DEBUG_LOGE( "Failed to get PNG header. Result: %d", headerResult );
		spng_ctx_free( spng );
		return;
	}

	enum spng_format sourceImageFormat;
	switch( ihdr.color_type )
	{
	case SPNG_COLOR_TYPE_GRAYSCALE:
		sourceImageFormat				= SPNG_FMT_G8;
		resEvent->result.image.format	= ImAppRendererFormat_R8;
		break;

	case SPNG_COLOR_TYPE_INDEXED:
	case SPNG_COLOR_TYPE_TRUECOLOR:
		sourceImageFormat				= SPNG_FMT_RGB8;
		resEvent->result.image.format	= ImAppRendererFormat_RGB8;
		break;

	case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
		sourceImageFormat				= SPNG_FMT_RGBA8;
		resEvent->result.image.format	= ImAppRendererFormat_RGBA8;
		break;

	case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
	default:
		{
			IMAPP_DEBUG_LOGE( "Not supported PNG format. Format: %d", ihdr.color_type );
			spng_ctx_free( spng );
			return;
		}
		break;
	}

	void* pixelData = ImUiMemoryAlloc( ressys->allocator, pixelDataSize );
	if( !pixelData )
	{
		IMAPP_DEBUG_LOGE( "Failed to allocate PNG pixel data. Size: %d", pixelDataSize );
		spng_ctx_free( spng );
		return;
	}

	const int decodeResult = spng_decode_image( spng, pixelData, pixelDataSize, sourceImageFormat, 0 );
	spng_ctx_free( spng );

	if( decodeResult != 0 )
	{
		IMAPP_DEBUG_LOGE( "Failed to decode PNG. Result: %d", decodeResult );
		ImUiMemoryFree( ressys->allocator, pixelData );
		return;
	}

	resEvent->result.image.width		= ihdr.width;
	resEvent->result.image.height		= ihdr.height;
	resEvent->result.image.data.data	= pixelData;
	resEvent->result.image.data.size	= pixelDataSize;

	resEvent->success = true;
}

static void ImAppResThreadHandleDecodeJpeg( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	// TODO
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

static ImUiHash ImAppResSysNameMapHash( const void* key )
{
	const ImAppRes* res = *(const ImAppRes**)key;
	return ImUiHashString( res->key.name, 0u );
}

static bool ImAppResSysNameMapIsKeyEquals( const void* lhs, const void* rhs )
{
	const ImAppRes* lhsRes = *(const ImAppRes**)lhs;
	const ImAppRes* rhsRes = *(const ImAppRes**)rhs;

	return ImUiStringViewIsEquals( lhsRes->key.name, rhsRes->key.name );
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

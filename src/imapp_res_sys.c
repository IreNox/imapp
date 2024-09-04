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
	ImAppFileWatcher*	watcher;

	ImAppResPak*		firstResPak;

	ImUiHashMap			nameMap;
	ImAppRes*			firstUnusedRes;

	ImUiHashMap			imageMap;

	ImAppThread*		thread;

	ImAppResEventQueue	sendQueue;
	ImAppResEventQueue	receiveQueue;
};

static void			ImAppResSysCloseInternal( ImAppResSys* ressys, ImAppResPak* pak );

static void			ImAppResSysHandleOpenResPak( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void			ImAppResSysHandleLoadResData( ImAppResSys* ressys, ImAppResEvent* resEvent );
static void			ImAppResSysHandleImage( ImAppResSys* ressys, ImAppResEvent* resEvent );

static ImAppRes*	ImAppResSysLoad( ImAppResPak* pak, uint16 resIndex );
static void			ImAppResSysUnload( ImAppResPak* pak, ImAppRes* res );

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
static ImUiHash		ImAppResSysImageMapHash( const void* key );
static bool			ImAppResSysImageMapIsKeyEquals( const void* lhs, const void* rhs );

static const ImAppResPakResource*	ImAppResPakResourceGet( const void* base, uint16_t index );
static ImUiStringView				ImAppResPakResourceGetName( const void* base, const ImAppResPakResource* res );
static const void*					ImAppResPakResourceGetHeader( const void* base, const ImAppResPakResource* res );

ImAppResSys* ImAppResSysCreate( ImUiAllocator* allocator, ImAppPlatform* platform, ImAppRenderer* renderer, ImUiContext* imui )
{
	ImAppResSys* ressys = IMUI_MEMORY_NEW_ZERO( allocator, ImAppResSys );

	ressys->allocator	= allocator;
	ressys->platform	= platform;
	ressys->imui		= imui;
	ressys->renderer	= renderer;
	ressys->watcher		= ImAppPlatformFileWatcherCreate( platform );

	if( !ImUiHashMapConstructSize( &ressys->nameMap, allocator, sizeof( ImAppRes* ), ImAppResSysNameMapHash, ImAppResSysNameMapIsKeyEquals, 64u ) ||
		!ImUiHashMapConstructSize( &ressys->imageMap, allocator, sizeof( ImAppRes* ), ImAppResSysImageMapHash, ImAppResSysImageMapIsKeyEquals, 64u ) ||
		!ImAppResEventQueueConstruct( ressys, &ressys->sendQueue, 16u ) ||
		!ImAppResEventQueueConstruct( ressys, &ressys->receiveQueue, 16u ) )
	{
		ImAppResSysDestroy( ressys );
		return NULL;
	}

	ressys->thread = ImAppPlatformThreadCreate( platform, "res sys", ImAppResThreadEntry, ressys );

	return ressys;
}

void ImAppResSysDestroy( ImAppResSys* ressys )
{
	if( ressys->thread )
	{
		const ImAppResEvent resEvent = { .type = ImAppResEventType_Quit };
		ImAppResEventQueuePush( ressys, &ressys->sendQueue, &resEvent );

		ImAppPlatformThreadDestroy( ressys->thread );
		ressys->thread = NULL;
	}

	for( uintsize i = ImUiHashMapFindFirstIndex( &ressys->imageMap ); i != IMUI_SIZE_MAX; i = ImUiHashMapFindNextIndex( &ressys->imageMap, i ) )
	{
		ImAppImage* image = *(ImAppImage**)ImUiHashMapGetEntry( &ressys->imageMap, i );
		ImAppResSysImageFree( ressys, image );
	}

	ImAppResEventQueueDestruct( ressys, &ressys->sendQueue );
	ImAppResEventQueueDestruct( ressys, &ressys->receiveQueue );

	ImUiHashMapDestruct( &ressys->imageMap );
	ImUiHashMapDestruct( &ressys->nameMap );

	ImAppPlatformFileWatcherDestroy( ressys->platform, ressys->watcher );

	ImUiMemoryFree( ressys->allocator, ressys );
}

void ImAppResSysUpdate( ImAppResSys* ressys )
{
	if( ressys->watcher )
	{
		ImAppFileWatchEvent watchEvent;
		if( ImAppPlatformFileWatcherPopEvent( ressys->watcher, &watchEvent ) )
		{
			ImAppResPak* pak;
			for( pak = ressys->firstResPak; pak; pak = pak->nextResPak )
			{
				if( strcmp( pak->resourceName, watchEvent.path ) == 0 )
				{
					break;
				}
			}

			if( pak )
			{
				const uintsize nameLength = strlen( pak->resourceName );

				ImAppResPak* newPak = (ImAppResPak*)ImUiMemoryAllocZero( ressys->allocator, sizeof( ImAppResPak ) + nameLength );
				newPak->ressys	= ressys;
				memcpy( newPak->resourceName, pak->resourceName, nameLength );

				ImAppResEvent resEvent;
				resEvent.type				= ImAppResEventType_OpenResPak;
				resEvent.data.pak.pak		= newPak;
				resEvent.data.pak.reloadPak	= pak;

				if( !ImAppResEventQueuePush( ressys, &ressys->sendQueue, &resEvent ) )
				{
					ImUiMemoryFree( ressys->allocator, newPak );
				}
			}
		}
	}

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

			case ImAppResEventType_Quit:
				return;
			}
		}
	}
}

static void ImAppResSysHandleOpenResPak( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	ImAppResPak* pak = resEvent->data.pak.pak;

	if( resEvent->data.pak.reloadPak )
	{
		if( !resEvent->success )
		{
			ImAppResSysCloseInternal( ressys, pak );
			return;
		}

		ImAppResPak* reloadPak = resEvent->data.pak.reloadPak;

		pak->prevResPak = reloadPak->prevResPak;
		pak->nextResPak = reloadPak->nextResPak;

		for( uintsize i = 0; i < pak->resourceCount; ++i )
		{
			pak->resources[ i ].key.pak = reloadPak;
		}

		ImAppResPak tempPak = *reloadPak;
		*reloadPak = *pak;
		*pak = tempPak;

		ImAppResSysCloseInternal( ressys, pak );
		pak = reloadPak;
	}
	else
	{
		if( !resEvent->success )
		{
			pak->state = ImAppResState_Error;
			return;
		}

		if( ressys->watcher )
		{
			char pakPath[ 1024u ];
			ImAppPlatformResourceGetPath( ressys->platform, pakPath, IMAPP_ARRAY_COUNT( pakPath ), pak->resourceName );

			ImAppPlatformFileWatcherAddPath( ressys->watcher, pakPath );
		}
	}

	for( uintsize i = 0; i < pak->resourceCount; ++i )
	{
		const ImAppRes* res = &pak->resources[ i ];
		ImUiHashMapInsert( &ressys->nameMap, &res );
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
			const ImAppResPakResource* sourceRes = ImAppResPakResourceGet( res->key.pak->metadata, res->key.index );
			const ImAppResPakTextureHeader* header = (const ImAppResPakTextureHeader*)ImAppResPakResourceGetHeader( res->key.pak->metadata, sourceRes );

			ImAppRendererFormat format = ImAppRendererFormat_RGBA8;
			switch( header->format )
			{
			case ImAppResPakTextureFormat_A8:		format = ImAppRendererFormat_R8; break;
			case ImAppResPakTextureFormat_PNG24:
			case ImAppResPakTextureFormat_JPEG:
			case ImAppResPakTextureFormat_RGB8:		format = ImAppRendererFormat_RGB8; break;
			case ImAppResPakTextureFormat_PNG32:
			case ImAppResPakTextureFormat_RGBA8:	format = ImAppRendererFormat_RGBA8; break;
			}

			res->data.texture.texture	= ImAppRendererTextureCreateFromMemory( ressys->renderer, resEvent->result.loadRes.data.data, header->width, header->height, format, header->flags );
			res->data.texture.width		= header->width;
			res->data.texture.height	= header->height;

			ImAppPlatformResourceFree( ressys->platform, resEvent->result.loadRes.data );

			if( !res->data.texture.texture )
			{
				res->state = ImAppResState_Error;
				return;
			}
		}
		break;

	case ImAppResPakType_Font:
		{
			ImAppResFontData* fontData = &res->data.font;

			const ImAppResPakResource* sourceRes = ImAppResPakResourceGet( res->key.pak->metadata, res->key.index );
			const ImAppResPakFontHeader* header = (const ImAppResPakFontHeader*)ImAppResPakResourceGetHeader( res->key.pak->metadata, sourceRes );

			ImUiFontParameters parameters;
			parameters.codepoints			= (const ImUiFontCodepoint*)resEvent->result.loadRes.data.data;
			parameters.codepointCount		= header->codepointCount;
			parameters.fontSize				= header->fontSize;
			parameters.image.textureData	= fontData->textureRes->data.texture.texture;
			parameters.image.width			= fontData->textureRes->data.texture.width;
			parameters.image.height			= fontData->textureRes->data.texture.height;
			parameters.image.uv.u0			= 0.0f;
			parameters.image.uv.v0			= 0.0f;
			parameters.image.uv.u1			= 1.0f;
			parameters.image.uv.v1			= 1.0f;
			parameters.isScalable			= false;
			parameters.lineGap				= header->fontSize;

			res->data.font.font				= ImUiFontCreate( ressys->imui, &parameters );

			ImAppPlatformResourceFree( ressys->platform, resEvent->result.loadRes.data );
		}
		break;

	case ImAppResPakType_Blob:
		res->data.blob.blob = resEvent->result.loadRes.data;
		break;

	case ImAppResPakType_Image:
	case ImAppResPakType_Skin:
	case ImAppResPakType_Theme:
	case ImAppResPakType_MAX:
		// no data
		break;
	}

	res->state = ImAppResState_Ready;
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
	image->data.textureData	= ImAppRendererTextureCreateFromMemory( ressys->renderer, result->data.data, result->width, result->height, result->format, 0u );
	image->data.width		= result->width;
	image->data.height		= result->height;
	image->data.uv.u0		= 0.0f;
	image->data.uv.v0		= 0.0f;
	image->data.uv.u1		= 1.0f;
	image->data.uv.v1		= 1.0f;

	ImUiMemoryFree( ressys->allocator, result->data.data );

	image->state = ImAppResState_Ready;
}

static ImAppRes* ImAppResSysLoad( ImAppResPak* pak, uint16 resIndex )
{
	if( pak->state != ImAppResState_Ready )
	{
		return NULL;
	}

	if( resIndex >= pak->resourceCount )
	{
		IMAPP_DEBUG_LOGW( "Could not find resource %d in '%s'", resIndex, pak->resourceName );
		return NULL;
	}

	ImAppRes* res = &pak->resources[ resIndex ];
	if( res->state == ImAppResState_Ready )
	{
		res->refCount++;
		//ImAppResSysChangeUsage( pak->ressys, res, ImAppResUsage_Used );
		return res;
	}

	const ImAppResPakResource* sourceRes = ImAppResPakResourceGet( pak->metadata, res->key.index );
	IMAPP_ASSERT( sourceRes );

	switch( res->key.type )
	{
	case ImAppResPakType_Texture:
		{
			ImAppResTextureData* textureData = &res->data.texture;

			if( !textureData->loading )
			{
				ImAppResEvent resEvent;
				resEvent.type			= ImAppResEventType_LoadResData;
				resEvent.data.res.res	= res;

				if( !ImAppResEventQueuePush( pak->ressys, &pak->ressys->sendQueue, &resEvent ) )
				{
					res->state = ImAppResState_Error;
					return NULL;
				}

				textureData->loading = true;
			}

			return res;
		}
		break;

	case ImAppResPakType_Image:
		{
			ImAppResImageData* imageData = &res->data.image;

			if( !imageData->textureRes )
			{
				ImAppRes* textureRes = ImAppResSysLoad( pak, sourceRes->textureIndex );
				if( !textureRes )
				{
					return res;
				}

				imageData->textureRes = textureRes;
			}

			if( imageData->textureRes->state == ImAppResState_Error )
			{
				res->state = ImAppResState_Error;
				return NULL;
			}
			else if( imageData->textureRes->state != ImAppResState_Ready )
			{
				return res;
			}

			const ImAppResPakImageHeader* header = (const ImAppResPakImageHeader*)ImAppResPakResourceGetHeader( pak->metadata, sourceRes );

			imageData->image.textureData	= imageData->textureRes->data.texture.texture;
			imageData->image.width			= header->width;
			imageData->image.height			= header->height;
			imageData->image.uv.u0			= (float)header->x / imageData->textureRes->data.texture.width;
			imageData->image.uv.v0			= (float)header->y / imageData->textureRes->data.texture.height;
			imageData->image.uv.u1			= (float)(header->x + header->width) / imageData->textureRes->data.texture.width;
			imageData->image.uv.v1			= (float)(header->y + header->height) / imageData->textureRes->data.texture.height;
		}
		break;

	case ImAppResPakType_Skin:
		{
			ImAppResSkinData* skinData = &res->data.skin;

			if( sourceRes->textureIndex != IMAPP_RES_PAK_INVALID_INDEX )
			{
				if( !skinData->textureRes )
				{
					ImAppRes* textureRes = ImAppResSysLoad( pak, sourceRes->textureIndex );
					if( !textureRes )
					{
						return res;
					}

					skinData->textureRes = textureRes;
				}

				if( skinData->textureRes->state == ImAppResState_Error )
				{
					res->state = ImAppResState_Error;
					return NULL;
				}
				else if( skinData->textureRes->state != ImAppResState_Ready )
				{
					return res;
				}

				const ImAppResPakSkinHeader* header = (const ImAppResPakSkinHeader*)ImAppResPakResourceGetHeader( pak->metadata, sourceRes );

				skinData->skin.textureData	= skinData->textureRes->data.texture.texture;
				skinData->skin.width		= header->width;
				skinData->skin.height		= header->height;
				skinData->skin.uv.u0		= (float)header->x / skinData->textureRes->data.texture.width;
				skinData->skin.uv.v0		= (float)header->y / skinData->textureRes->data.texture.height;
				skinData->skin.uv.u1		= (float)(header->x + header->width) / skinData->textureRes->data.texture.width;
				skinData->skin.uv.v1		= (float)(header->y + header->height) / skinData->textureRes->data.texture.height;
				skinData->skin.border		= ImUiBorderCreate( header->top, header->left, header->bottom, header->right );
			}
		}
		break;

	case ImAppResPakType_Font:
		{
			ImAppResFontData* fontData = &res->data.font;

			if( sourceRes->textureIndex != IMAPP_RES_PAK_INVALID_INDEX )
			{
				if( !fontData->textureRes )
				{
					ImAppRes* textureRes = ImAppResSysLoad( pak, sourceRes->textureIndex );
					if( !textureRes )
					{
						return res;
					}

					fontData->textureRes = textureRes;
				}

				if( fontData->textureRes->state == ImAppResState_Error )
				{
					res->state = ImAppResState_Error;
					return NULL;
				}
				else if( fontData->textureRes->state != ImAppResState_Ready )
				{
					return res;
				}

				if( !fontData->loading )
				{
					ImAppResEvent resEvent;
					resEvent.type			= ImAppResEventType_LoadResData;
					resEvent.data.res.res	= res;

					if( !ImAppResEventQueuePush( pak->ressys, &pak->ressys->sendQueue, &resEvent ) )
					{
						res->state = ImAppResState_Error;
						return NULL;
					}

					fontData->loading = true;
				}

				return res;
			}
			else
			{
				// TODO
				fontData->font = ImAppResSysFontCreateSystem( res->key.pak->ressys, "arial.ttf", 16.0f, &fontData->texture );
			}
		}
		break;

	case ImAppResPakType_Theme:
		{
			ImAppResThemeData* themeData = &res->data.theme;

			const ImAppResPakThemeHeader* header = (const ImAppResPakThemeHeader*)ImAppResPakResourceGetHeader( pak->metadata, sourceRes );

			bool ready = true;
			const uint16* referencedIndices = (const uint16*)&header[ 1u ];
			for( uintsize i = 0; i < header->referencedCount; ++i )
			{
				ImAppRes* refRes = ImAppResSysLoad( pak, referencedIndices[ i ] );
				if( !refRes )
				{
					ready = false;
					continue;
				}

				ready &= (refRes->state == ImAppResState_Ready);
			}

			if( !ready )
			{
				return res;
			}

			ImUiToolboxConfig* config = IMUI_MEMORY_NEW_ZERO( pak->ressys->allocator, ImUiToolboxConfig );

			if( header->fontIndex != IMAPP_RES_PAK_INVALID_INDEX )
			{
				ImAppRes* fontRes = ImAppResSysLoad( pak, header->fontIndex );
				IMAPP_ASSERT( fontRes && fontRes->state == ImAppResState_Ready );
				config->font = fontRes->data.font.font;
			}

			for( uintsize i = 0; i < IMAPP_ARRAY_COUNT( themeData->config->colors ); ++i )
			{
				config->colors[ i ] = header->colors[ i ];
			}

			for( uintsize i = 0; i < IMAPP_ARRAY_COUNT( themeData->config->skins ); ++i )
			{
				if( header->skinIndices[ i ] == IMAPP_RES_PAK_INVALID_INDEX )
				{
					continue;
				}

				ImAppRes* skinRes = ImAppResSysLoad( pak, header->skinIndices[ i ] );
				IMAPP_ASSERT( skinRes && skinRes->state == ImAppResState_Ready );
				config->skins[ i ] = skinRes->data.skin.skin;
			}

			for( uintsize i = 0; i < IMAPP_ARRAY_COUNT( themeData->config->icons ); ++i )
			{
				if( header->iconIndices[ i ] == IMAPP_RES_PAK_INVALID_INDEX )
				{
					continue;
				}

				ImAppRes* imageRes = ImAppResSysLoad( pak, header->iconIndices[ i ] );
				IMAPP_ASSERT( imageRes && imageRes->state == ImAppResState_Ready );
				config->icons[ i ] = imageRes->data.image.image;
			}

			config->button		= header->button;
			config->checkBox	= header->checkBox;
			config->slider		= header->slider;
			config->textEdit	= header->textEdit;
			config->progressBar	= header->progressBar;
			config->scrollArea	= header->scrollArea;
			config->list		= header->list;
			config->dropDown	= header->dropDown;
			config->popup		= header->popup;

			themeData->config = config;
		}
		break;

	case ImAppResPakType_Blob:
		// TODO
		break;

	case ImAppResPakType_MAX:
		break;
	}

	res->refCount++;
	res->state = ImAppResState_Ready;
	//ImAppResSysChangeUsage( pak->ressys, res, ImAppResUsage_Used );
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
	resEvent.type				= ImAppResEventType_OpenResPak;
	resEvent.data.pak.pak		= pak;
	resEvent.data.pak.reloadPak	= NULL;

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

static void ImAppResSysCloseInternal( ImAppResSys* ressys, ImAppResPak* pak )
{
	for( uintsize i = 0; i < pak->resourceCount; ++i )
	{
		ImAppRes* res = &pak->resources[ i ];
		ImAppResSysUnload( pak, res );
		ImUiHashMapRemove( &ressys->nameMap, &res );
	}

	ImAppBlob metadataBlob ={ pak->metadata, pak->metadataSize };
	ImAppPlatformResourceFree( ressys->platform, metadataBlob );

	ImUiMemoryFree( ressys->allocator, pak->resources );
	ImUiMemoryFree( ressys->allocator, pak );

	ImUiToolboxConfig config;
	ImUiToolboxFillDefaultConfig( &config, NULL );
	ImUiToolboxSetConfig( &config );
}

void ImAppResSysClose( ImAppResSys* ressys, ImAppResPak* pak )
{
	if( ressys->watcher )
	{
		char pakPath[ 1024u ];
		ImAppPlatformResourceGetPath( ressys->platform, pakPath, IMAPP_ARRAY_COUNT( pakPath ), pak->resourceName );

		ImAppPlatformFileWatcherRemovePath( ressys->watcher, pakPath );
	}

	if( pak->nextResPak )
	{
		pak->nextResPak->prevResPak = pak->prevResPak;
	}

	if( pak->prevResPak )
	{
		pak->prevResPak->nextResPak = pak->nextResPak;
	}

	if( pak == ressys->firstResPak )
	{
		ressys->firstResPak = pak->nextResPak;
	}

	ImAppResSysCloseInternal( ressys, pak );
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
	if( !pak || pak->state != ImAppResState_Ready )
	{
		return IMAPP_RES_PAK_INVALID_INDEX;
	}

	ImAppResKey key;
	key.name	= ImUiStringViewCreate( name );
	key.type	= type;
	key.pak		= (ImAppResPak*)pak;

	ImAppResKey* keyEntry = &key;
	const ImAppRes** resKey = (const ImAppRes**)ImUiHashMapFind( &pak->ressys->nameMap, &keyEntry );
	if( resKey )
	{
		return (*resKey)->key.index;
	}

	return IMAPP_RES_PAK_INVALID_INDEX;
}

const ImUiImage* ImAppResPakGetImage( ImAppResPak* pak, const char* name )
{
	const uint16 resIndex = ImAppResPakFindResourceIndex( pak, ImAppResPakType_Image, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return NULL;
	}

	return ImAppResPakGetImageIndex( pak, resIndex );
}

const ImUiImage* ImAppResPakGetImageIndex( ImAppResPak* pak, uint16_t resIndex )
{
	ImAppRes* res = ImAppResSysLoad( pak, resIndex );
	if( !res || res->state != ImAppResState_Ready )
	{
		return NULL;
	}

	IMAPP_ASSERT( res->key.type == ImAppResPakType_Image );
	return &res->data.image.image;
}

const ImUiSkin* ImAppResPakGetSkin( ImAppResPak* pak, const char* name )
{
	const uint16 resIndex = ImAppResPakFindResourceIndex( pak, ImAppResPakType_Skin, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return NULL;
	}

	return ImAppResPakGetSkinIndex( pak, resIndex );
}

const ImUiSkin* ImAppResPakGetSkinIndex( ImAppResPak* pak, uint16_t resIndex )
{
	ImAppRes* res = ImAppResSysLoad( pak, resIndex );
	if( !res || res->state != ImAppResState_Ready )
	{
		return NULL;
	}

	IMAPP_ASSERT( res->key.type == ImAppResPakType_Skin );
	return &res->data.skin.skin;
}

ImUiFont* ImAppResPakGetFont( ImAppResPak* pak, const char* name )
{
	const uint16 resIndex = ImAppResPakFindResourceIndex( pak, ImAppResPakType_Font, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return NULL;
	}

	return ImAppResPakGetFontIndex( pak, resIndex );
}

ImUiFont* ImAppResPakGetFontIndex( ImAppResPak* pak, uint16_t resIndex )
{
	ImAppRes* res = ImAppResSysLoad( pak, resIndex );
	if( !res || res->state != ImAppResState_Ready )
	{
		return NULL;
	}

	IMAPP_ASSERT( res->key.type == ImAppResPakType_Font );
	return res->data.font.font;
}

const ImUiToolboxConfig* ImAppResPakGetTheme( ImAppResPak* pak, const char* name )
{
	const uint16 resIndex = ImAppResPakFindResourceIndex( pak, ImAppResPakType_Theme, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return NULL;
	}

	return ImAppResPakGetThemeIndex( pak, resIndex );
}

const ImUiToolboxConfig* ImAppResPakGetThemeIndex( ImAppResPak* pak, uint16_t resIndex )
{
	ImAppRes* res = ImAppResSysLoad( pak, resIndex );
	if( !res || res->state != ImAppResState_Ready )
	{
		return NULL;
	}

	IMAPP_ASSERT( res->key.type == ImAppResPakType_Theme );
	return res->data.theme.config;
}

ImAppBlob ImAppResPakGetBlob( ImAppResPak* pak, const char* name )
{
	const uint16 resIndex = ImAppResPakFindResourceIndex( pak, ImAppResPakType_Blob, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	return ImAppResPakGetBlobIndex( pak, resIndex );
}

ImAppBlob ImAppResPakGetBlobIndex( ImAppResPak* pak, uint16_t resIndex )
{
	ImAppRes* res = ImAppResSysLoad( pak, resIndex );
	if( !res || res->state != ImAppResState_Ready )
	{
		const ImAppBlob result = { NULL, 0u };
		return  result;
	}

	IMAPP_ASSERT( res->key.type == ImAppResPakType_Blob );
	return res->data.blob.blob;
}

void ImAppResPakActivateTheme( ImAppResPak* pak, const char* name )
{
	const uint16 resIndex = ImAppResPakFindResourceIndex( pak, ImAppResPakType_Theme, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return;
	}

	ImAppRes* res = ImAppResSysLoad( pak, resIndex );
	if( !res || res->state != ImAppResState_Ready )
	{
		return;
	}

	ImUiToolboxSetConfig( res->data.theme.config );
}

ImAppImage* ImAppResSysImageCreateRaw( ImAppResSys* ressys, const void* pixelData, int width, int height )
{
	ImAppImage* image = IMUI_MEMORY_NEW( ressys->allocator, ImAppImage );
	image->resourceName		= ImUiStringViewCreateEmpty();
	image->data.textureData	= ImAppRendererTextureCreateFromMemory( ressys->renderer, pixelData, width, height, ImAppRendererFormat_RGBA8, 0u );
	image->data.width		= width;
	image->data.height		= height;
	image->data.uv.u0		= 0.0f;
	image->data.uv.v0		= 0.0f;
	image->data.uv.u1		= 1.0f;
	image->data.uv.v1		= 1.0f;
	image->state			= ImAppResState_Ready;

	if( !image->data.textureData )
	{
		ImUiMemoryFree( ressys->allocator, image );
		return NULL;
	}

	return image;
}

ImAppImage* ImAppResSysImageLoadResource( ImAppResSys* ressys, const char* resourceName )
{
	const ImUiStringView resourceNameView = ImUiStringViewCreate( resourceName );
	const ImUiStringView* resourceNameViewEntry = &resourceNameView;

	bool isNew;
	ImAppImage** imageEntry = (ImAppImage**)ImUiHashMapInsertNew( &ressys->imageMap, &resourceNameViewEntry, &isNew );
	if( isNew )
	{
		ImAppImage* image = (ImAppImage*)ImUiMemoryAllocZero( ressys->allocator, sizeof( *image ) + resourceNameView.length + 1u );

		char* targetResourceName = (char*)&image[ 1u ];
		memcpy( targetResourceName, resourceName, resourceNameView.length );
		targetResourceName[ resourceNameView.length ] = '\0';

		image->resourceName.data	= targetResourceName;
		image->resourceName.length	= resourceNameView.length;

		ImAppResEvent loadEvent;
		loadEvent.type				= ImAppResEventType_LoadImage;
		loadEvent.data.image.image	= image;

		if( !ImAppResEventQueuePush( ressys, &ressys->sendQueue, &loadEvent ) )
		{
			ImUiHashMapRemove( &ressys->imageMap, &image );
			ImUiMemoryFree( ressys->allocator, image );
			return NULL;
		}

		*imageEntry = image;
	}

	return *imageEntry;
}

ImAppImage* ImAppResSysImageCreatePng( ImAppResSys* ressys, const void* imageData, uintsize imageDataSize )
{
	ImAppImage* image = IMUI_MEMORY_NEW_ZERO( ressys->allocator, ImAppImage );

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
	ImAppImage* image = IMUI_MEMORY_NEW_ZERO( ressys->allocator, ImAppImage );

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
	if( image->data.textureData )
	{
		ImAppRendererTextureDestroy( ressys->renderer, (ImAppRendererTexture*)image->data.textureData );
	}

	ImUiMemoryFree( ressys->allocator, image );
}

ImUiFont* ImAppResSysFontCreateSystem( ImAppResSys* ressys, const char* fontName, float fontSize, ImAppRendererTexture** texture )
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

	ImUiFontTrueTypeImage* ttfImage = ImUiFontTrueTypeDataGenerateTextureData( ttf, fontSize, textureData, width * height, width, height );
	if( !ttfImage )
	{
		ImUiFontTrueTypeDataDestroy( ttf );
		ImUiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	*texture = ImAppRendererTextureCreateFromMemory( ressys->renderer, textureData, width, height, ImAppRendererFormat_R8, ImAppResPakTextureFlags_Font );
	free( textureData );

	if( !*texture )
	{
		ImUiFontTrueTypeDataDestroy( ttf );
		ImUiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	ImUiImage uiImage;
	uiImage.textureData	= *texture;
	uiImage.width		= width;
	uiImage.height		= height;
	uiImage.uv.u0		= 0.0f;
	uiImage.uv.v0		= 0.0f;
	uiImage.uv.u1		= 1.0f;
	uiImage.uv.v1		= 1.0f;

	ImUiFont* font = ImUiFontCreateTrueType( ressys->imui, ttfImage, uiImage );

	ImUiFontTrueTypeDataDestroy( ttf );
	ImUiMemoryFree( ressys->allocator, fontBlob.data );

	return font;
}

static void ImAppResThreadEntry( void* arg )
{
	ImAppResSys* ressys = (ImAppResSys*)arg;

	bool running = true;
	ImAppResEvent resEvent;
	while( running )
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

		case ImAppResEventType_Quit:
			running = false;
			break;
		}

		ImAppResEventQueuePush( ressys, &ressys->receiveQueue, &resEvent );
	}
}

static void ImAppResSysUnload( ImAppResPak* pak, ImAppRes* res )
{
	ImAppResSys* ressys = res->key.pak->ressys;

	switch( res->key.type )
	{
	case ImAppResPakType_Texture:
		{
			if( res->data.texture.texture )
			{
				ImAppRendererTextureDestroy( ressys->renderer, res->data.texture.texture );
				res->data.texture.texture	= NULL;
				res->data.texture.width		= 0u;
				res->data.texture.height	= 0u;
				res->data.texture.loading	= false;
			}
		}
		break;

	case ImAppResPakType_Font:
		{
			if( res->data.font.font )
			{
				ImUiFontDestroy( ressys->imui, res->data.font.font );
				res->data.font.font = NULL;
			}

			if( res->data.font.texture )
			{
				ImAppRendererTextureDestroy( ressys->renderer, res->data.font.texture );
				res->data.font.texture = NULL;
			}
		}
		break;

	case ImAppResPakType_Theme:
		{
			if( res->data.theme.config )
			{
				ImUiMemoryFree( ressys->allocator, res->data.theme.config );
				res->data.theme.config = NULL;
			}
		}
		break;

	case ImAppResPakType_Blob:
		{
			ImAppPlatformResourceFree( ressys->platform, res->data.blob.blob );
			res->data.blob.blob.data	= NULL;
			res->data.blob.blob.size	= 0u;
		}
		break;

	case ImAppResPakType_Image:
	case ImAppResPakType_Skin:
	case ImAppResPakType_MAX:
		break;
	}

	memset( &res->data, 0, sizeof( res->data ) );
	res->state		= ImAppResState_Loading;
	res->refCount	= 0u;
}

static void ImAppResThreadHandleOpenResPak( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	ImAppResPak* pak = resEvent->data.pak.pak;

	ImAppFile* file = ImAppPlatformResourceOpen( ressys->platform, pak->resourceName );
	if( !file )
	{
		IMAPP_DEBUG_LOGE( "Failed to open '%s' ResPak.", pak->resourceName );
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
		targetRes->key.name		= ImAppResPakResourceGetName( pak->metadata, sourceRes );
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
#if IMAPP_ENABLED( IMAPP_DEBUG )
		const ImUiStringView resName = ImAppResPakResourceGetName( pak->metadata, sourceRes );
#endif
		IMAPP_DEBUG_LOGE( "Failed to load data of resource '%s' in pak '%s'.", resName.data, pak->resourceName );
		return;
	}

	resEvent->result.loadRes.data = resData;
	resEvent->success = true;
}

static void ImAppResThreadHandleImageLoad( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	ImAppImage* image = resEvent->data.image.image;

	const ImAppBlob data = ImAppPlatformResourceLoad( ressys->platform, image->resourceName.data );
	if( !data.data )
	{
		IMAPP_DEBUG_LOGE( "Failed to load image '%s'.", image->resourceName.data );
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

		ImAppPlatformResourceFree( ressys->platform, data );

		if( !decodeEvent.success )
		{
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
	queue->semaphore	= ImAppPlatformSemaphoreCreate( ressys->platform );
	queue->mutex		= ImAppPlatformMutexCreate( ressys->platform );
	queue->events		= IMUI_MEMORY_ARRAY_NEW( ressys->allocator, ImAppResEvent, initSize );
	queue->top			= 0u;
	queue->bottom		= 0u;
	queue->count		= 0u;
	queue->capacity		= initSize;

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
	queue->events	= NULL;
	queue->top		= 0u;
	queue->bottom	= 0u;
	queue->count	= 0u;
	queue->capacity	= 0u;

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

	if( queue->count + 1u > queue->capacity )
	{
		const uintsize nextCapacity = IMUI_MAX( IMUI_DEFAULT_ARRAY_CAPACITY, queue->capacity << 1u );
		const uintsize newSize		= nextCapacity * sizeof( ImAppResEvent );

		ImAppResEvent* newEvents = (ImAppResEvent*)ImUiMemoryAlloc( ressys->allocator, newSize );
		if( !newEvents )
		{
			ImAppPlatformMutexUnlock( queue->mutex );
			return false;
		}

		for(uintsize i = 0; i < queue->count; ++i )
		{
			const uintsize index = (queue->top + i) % queue->capacity;
			newEvents[ i ] = queue->events[ index ];
		}

		ImUiMemoryFree( ressys->allocator, queue->events );

		queue->events	= newEvents;
		queue->top		= 0u;
		queue->bottom	= queue->count;
		queue->capacity	= nextCapacity;
	}

	const uintsize index = queue->bottom;
	queue->bottom = (queue->bottom + 1u) % queue->capacity;
	queue->count++;

	queue->events[ index ] = *resEvent;

	ImAppPlatformMutexUnlock( queue->mutex );

	ImAppPlatformSemaphoreInc( queue->semaphore );
	return true;
}

static bool ImAppResEventQueuePop( ImAppResEventQueue* queue, ImAppResEvent* outEvent, bool wait )
{
	if( !ImAppPlatformSemaphoreDec( queue->semaphore, wait ) )
	{
		return false;
	}

	ImAppPlatformMutexLock( queue->mutex );

	if( queue->count == 0u )
	{
		ImAppPlatformMutexUnlock( queue->mutex );
		return false;
	}

	*outEvent = queue->events[ queue->top ];

	queue->top = (queue->top + 1u) % queue->capacity;
	queue->count--;

	ImAppPlatformMutexUnlock( queue->mutex );
	return true;
}

static ImUiHash ImAppResSysNameMapHash( const void* key )
{
	const ImAppRes* res = *(const ImAppRes**)key;

	const uintsize pakInt = (uintsize)res->key.pak;
	ImUiHash hash = res->key.type;
	hash ^= pakInt & 0xffffffffu;
#if IMAPP_ENABLED( IMAPP_POINTER_64 )
	hash ^= pakInt >> 32u;
#endif

	return ImUiHashMix( ImUiHashString( res->key.name, 0u ), hash );
}

static bool ImAppResSysNameMapIsKeyEquals( const void* lhs, const void* rhs )
{
	const ImAppRes* lhsRes = *(const ImAppRes**)lhs;
	const ImAppRes* rhsRes = *(const ImAppRes**)rhs;

	return lhsRes->key.pak == rhsRes->key.pak &&
		lhsRes->key.type == rhsRes->key.type &&
		ImUiStringViewIsEquals( lhsRes->key.name, rhsRes->key.name );
}

static ImUiHash ImAppResSysImageMapHash( const void* key )
{
	const ImAppImage* image = *(const ImAppImage**)key;

	return ImUiHashString( image->resourceName, 0u );
}

static bool ImAppResSysImageMapIsKeyEquals( const void* lhs, const void* rhs )
{
	const ImAppImage* lhsImage = *(const ImAppImage**)lhs;
	const ImAppImage* rhsImage = *(const ImAppImage**)rhs;

	return ImUiStringViewIsEquals( lhsImage->resourceName, rhsImage->resourceName );
}

static const ImAppResPakResource* ImAppResPakResourceGet( const void* base, uint16_t index )
{
	const ImAppResPakHeader* header	= (const ImAppResPakHeader*)base;
	if( index >= header->resourceCount )
	{
		return NULL;
	}

	const byte* bytes = (const byte*)base;
	bytes += sizeof( ImAppResPakHeader );
	bytes += sizeof( ImAppResPakResource ) * index;

	return (const ImAppResPakResource*)bytes;
}

static ImUiStringView ImAppResPakResourceGetName( const void* base, const ImAppResPakResource* res )
{
	const byte* bytes = (const byte*)base;
	bytes += res->nameOffset;

	return ImUiStringViewCreateLength( (const char*)bytes, res->nameLength );
}

static const void* ImAppResPakResourceGetHeader( const void* base, const ImAppResPakResource* res )
{
	const byte* bytes = (const byte*)base;
	bytes += res->headerOffset;

	return bytes;
}

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

void ImAppResSysUpdate( ImAppResSys* ressys, bool wait )
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
		while( ImAppResEventQueuePop( &ressys->receiveQueue, &resEvent, wait ) )
		{
			wait = false;
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

			if( res->key.pak->memoryData == NULL )
			{
				ImAppPlatformResourceFree( ressys->platform, resEvent->result.loadRes.data );
			}

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
			parameters.image.textureHandle	= (uint64_t)fontData->textureRes->data.texture.texture;
			parameters.image.width			= fontData->textureRes->data.texture.width;
			parameters.image.height			= fontData->textureRes->data.texture.height;
			parameters.image.uv.u0			= 0.0f;
			parameters.image.uv.v0			= 0.0f;
			parameters.image.uv.u1			= 1.0f;
			parameters.image.uv.v1			= 1.0f;
			parameters.isScalable			= false;
			parameters.lineGap				= header->fontSize;

			res->data.font.font				= ImUiFontCreate( ressys->imui, &parameters );

			if( res->key.pak->memoryData == NULL )
			{
				ImAppPlatformResourceFree( ressys->platform, resEvent->result.loadRes.data );
			}
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
	image->data.textureHandle	= (uint64_t)ImAppRendererTextureCreateFromMemory( ressys->renderer, result->data.data, result->width, result->height, result->format, 0u );
	image->data.width			= result->width;
	image->data.height			= result->height;
	image->data.uv.u0			= 0.0f;
	image->data.uv.v0			= 0.0f;
	image->data.uv.u1			= 1.0f;
	image->data.uv.v1			= 1.0f;

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

			imageData->image.textureHandle	= (uint64_t)imageData->textureRes->data.texture.texture;
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

				skinData->skin.textureHandle	= (uint64_t)skinData->textureRes->data.texture.texture;
				skinData->skin.width			= header->width;
				skinData->skin.height			= header->height;
				skinData->skin.uv.u0			= (float)header->x / skinData->textureRes->data.texture.width;
				skinData->skin.uv.v0			= (float)header->y / skinData->textureRes->data.texture.height;
				skinData->skin.uv.u1			= (float)(header->x + header->width) / skinData->textureRes->data.texture.width;
				skinData->skin.uv.v1			= (float)(header->y + header->height) / skinData->textureRes->data.texture.height;
				skinData->skin.border			= ImUiBorderCreate( header->top, header->left, header->bottom, header->right );
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

			const ImAppResPakThemeHeader* header	= (const ImAppResPakThemeHeader*)ImAppResPakResourceGetHeader( pak->metadata, sourceRes );
			const byte* headerEnd					= (const byte*)header + sourceRes->headerSize;

			const uint16* referencedIndices = (const uint16*)&header[ 1u ];
			if( referencedIndices + header->referencedCount > (const uint16*)headerEnd )
			{
				IMAPP_DEBUG_LOGE( "Theme header too small for references." );
				res->state = ImAppResState_Error;
				return NULL;
			}

			bool ready = true;
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

			ImAppTheme* theme = IMUI_MEMORY_NEW_ZERO( pak->ressys->allocator, ImAppTheme );
			ImUiToolboxThemeFillDefault( &theme->uiTheme, NULL );
			ImAppWindowThemeFillDefault( &theme->windowTheme );

			const ImUiToolboxThemeReflection themeReflection = ImUiToolboxThemeReflectionGet();
			const ImUiToolboxThemeReflection windowReflection = ImAppWindowThemeReflectionGet();

			const ImAppResPakThemeField* fields = (const ImAppResPakThemeField*)&referencedIndices[ header->referencedCount ];
			if( fields + header->themeFieldCount > (const ImAppResPakThemeField*)headerEnd )
			{
				IMAPP_DEBUG_LOGE( "Theme header too small for fields." );
				res->state = ImAppResState_Error;
				return NULL;
			}

			const byte* fieldData = (const byte*)&fields[ header->themeFieldCount ];
			for( uintsize fieldIndex = 0u; fieldIndex < header->themeFieldCount; ++fieldIndex )
			{
				const ImAppResPakThemeField* field = &fields[ fieldIndex ];

				uintsize fieldSize = 0u;
				switch( field->type )
				{
				case ImUiToolboxThemeReflectionType_Color:	fieldSize = sizeof( ImUiColor ); break;
				case ImUiToolboxThemeReflectionType_Skin:	fieldSize = sizeof( uint16 ); break;
				case ImUiToolboxThemeReflectionType_Image:	fieldSize = sizeof( uint16 ); break;
				case ImUiToolboxThemeReflectionType_Font:	fieldSize = sizeof( uint16 ); break;
				case ImUiToolboxThemeReflectionType_Size:	fieldSize = sizeof( ImUiSize ); break;
				case ImUiToolboxThemeReflectionType_Border:	fieldSize = sizeof( ImUiBorder ); break;
				case ImUiToolboxThemeReflectionType_Float:	fieldSize = sizeof( float ); break;
				case ImUiToolboxThemeReflectionType_Double:	fieldSize = sizeof( double ); break;
				case ImUiToolboxThemeReflectionType_UInt32:	fieldSize = sizeof( uint32 ); break;
				}

				if( fieldData + fieldSize > headerEnd )
				{
					IMAPP_DEBUG_LOGE( "Theme header too small for field data 0x%08x.", field->nameHash );
					res->state = ImAppResState_Error;
					return NULL;
				}

				byte* targetBase = NULL;
				byte* targetBaseEnd = NULL;
				const ImUiToolboxThemeReflection* reflection = NULL;
				if( field->base == ImAppResPakThemeFieldBase_UiTheme )
				{
					targetBase		= (byte*)&theme->uiTheme;
					targetBaseEnd	= targetBase + sizeof( theme->uiTheme );
					reflection		= &themeReflection;
				}
				else if( field->base == ImAppResPakThemeFieldBase_WindowTheme )
				{
					targetBase		= (byte*)&theme->windowTheme;
					targetBaseEnd	= targetBase + sizeof( theme->windowTheme );
					reflection		= &windowReflection;
				}
				else
				{
					IMAPP_DEBUG_LOGW( "Theme field base is invalid." );
					fieldData += fieldSize;
					continue;
				}

				const ImUiToolboxThemeReflectionField* reflectionField = NULL;
				for( uintsize reflectionIndex = 0u; reflectionIndex < reflection->count; ++reflectionIndex )
				{
					const ImUiHash nameHash = ImUiHashCreate( reflection->fields[ reflectionIndex ].name, strlen( reflection->fields[ reflectionIndex ].name ) );
					if( nameHash != field->nameHash )
					{
						continue;
					}

					reflectionField = &reflection->fields[ reflectionIndex ];
					break;
				}

				if( !reflectionField )
				{
					IMAPP_DEBUG_LOGW( "Could not find theme field with hash 0x%08x", field->nameHash );
					fieldData += fieldSize;
					continue;
				}
				else if( reflectionField->type != field->type )
				{
					IMAPP_DEBUG_LOGW( "Theme field type is invalid." );
					fieldData += fieldSize;
					continue;
				}

				byte* targetData = targetBase + reflectionField->offset;
				if( targetData + fieldSize > targetBaseEnd )
				{
					IMAPP_DEBUG_LOGW( "Theme field offset out of range." );
					fieldData += fieldSize;
					continue;
				}

				switch( field->type )
				{
				case ImUiToolboxThemeReflectionType_Color:
					*(ImUiColor*)targetData = *(const ImUiColor*)fieldData;
					break;

				case ImUiToolboxThemeReflectionType_Skin:
				case ImUiToolboxThemeReflectionType_Image:
				case ImUiToolboxThemeReflectionType_Font:
					{
						const uint16 fieldResIndex = *(const uint16*)fieldData;
						if( fieldResIndex == IMAPP_RES_PAK_INVALID_INDEX )
						{
							fieldData += fieldSize;
							continue;
						}

						ImAppResPakType fieldResType = ImAppResPakType_MAX;
						switch( field->type )
						{
						case ImUiToolboxThemeReflectionType_Skin:	fieldResType = ImAppResPakType_Skin; break;
						case ImUiToolboxThemeReflectionType_Image:	fieldResType = ImAppResPakType_Image; break;
						case ImUiToolboxThemeReflectionType_Font:	fieldResType = ImAppResPakType_Font; break;
						}

						ImAppRes* fieldRes = ImAppResSysLoad( pak, fieldResIndex );
						if( !fieldRes ||
							fieldRes->key.type != fieldResType ||
							fieldRes->state != ImAppResState_Ready )
						{
							IMAPP_DEBUG_LOGE( "Theme resource %d for field '%s' is in the wrong state.", fieldResIndex, reflectionField->name );
							res->state = ImAppResState_Error;
							return NULL;
						}

						switch( field->type )
						{
						case ImUiToolboxThemeReflectionType_Skin:
							*(ImUiSkin*)targetData = fieldRes->data.skin.skin;
							break;

						case ImUiToolboxThemeReflectionType_Image:
							*(ImUiImage*)targetData = fieldRes->data.image.image;
							break;

						case ImUiToolboxThemeReflectionType_Font:
							*(ImUiFont**)targetData = fieldRes->data.font.font;
							break;
						}
					}
					break;

				case ImUiToolboxThemeReflectionType_Size:
					*(ImUiSize*)targetData = *(const ImUiSize*)fieldData;
					break;

				case ImUiToolboxThemeReflectionType_Border:
					*(ImUiBorder*)targetData = *(const ImUiBorder*)fieldData;
					break;

				case ImUiToolboxThemeReflectionType_Float:
					*(float*)targetData = *(const float*)fieldData;
					break;

				case ImUiToolboxThemeReflectionType_Double:
					*(double*)targetData = *(const double*)fieldData;
					break;

				case ImUiToolboxThemeReflectionType_UInt32:
					*(uint32*)targetData = *(const uint32*)fieldData;
					break;
				}

				fieldData += fieldSize;
			}

			themeData->theme = theme;
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
	// TODO
	IMAPP_USE( ressys );
	return false;
}

ImAppResPak* ImAppResSysAdd( ImAppResSys* ressys, const void* pakData, uintsize dataLength )
{
	ImAppResPak* pak = (ImAppResPak*)ImUiMemoryAllocZero( ressys->allocator, sizeof( ImAppResPak ) );
	pak->ressys			= ressys;
	pak->memoryData		= (const byte*)pakData;
	pak->memoryDataSize	= dataLength;

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

	if( pak->memoryData == NULL )
	{
		ImAppBlob metadataBlob;
		metadataBlob.data = pak->metadata;
		metadataBlob.size = pak->metadataSize;
		ImAppPlatformResourceFree( ressys->platform, metadataBlob );
	}

	ImUiMemoryFree( ressys->allocator, pak->resources );
	ImUiMemoryFree( ressys->allocator, pak );

	ImUiToolboxTheme config;
	ImUiToolboxThemeFillDefault( &config, NULL );
	ImUiToolboxThemeSet( &config );
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

ImAppResState ImAppResPakLoadResourceIndex( ImAppResPak* pak, uint16_t resIndex )
{
	if( resIndex >= pak->resourceCount )
	{
		return false;
	}

	const ImAppRes* res = ImAppResSysLoad( pak, resIndex );
	if( !res )
	{
		return ImAppResState_Error;
	}

	return res->state;
}

ImAppResState ImAppResPakLoadResourceName( ImAppResPak* pak, ImAppResPakType type, const char* name )
{
	const uint16 resIndex = ImAppResPakFindResourceIndex( pak, type, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return ImAppResState_Error;
	}

	return ImAppResPakLoadResourceIndex( pak, resIndex );
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

const ImAppTheme* ImAppResPakGetTheme( ImAppResPak* pak, const char* name )
{
	const uint16 resIndex = ImAppResPakFindResourceIndex( pak, ImAppResPakType_Theme, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return NULL;
	}

	return ImAppResPakGetThemeIndex( pak, resIndex );
}

const ImAppTheme* ImAppResPakGetThemeIndex( ImAppResPak* pak, uint16_t resIndex )
{
	ImAppRes* res = ImAppResSysLoad( pak, resIndex );
	if( !res || res->state != ImAppResState_Ready )
	{
		return NULL;
	}

	IMAPP_ASSERT( res->key.type == ImAppResPakType_Theme );
	return res->data.theme.theme;
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

ImAppImage* ImAppResSysImageCreateRaw( ImAppResSys* ressys, const void* pixelData, int width, int height )
{
	ImAppImage* image = IMUI_MEMORY_NEW( ressys->allocator, ImAppImage );
	image->resourceName			= ImUiStringViewCreateEmpty();
	image->data.textureHandle	= (uint64_t)ImAppRendererTextureCreateFromMemory( ressys->renderer, pixelData, width, height, ImAppRendererFormat_RGBA8, 0u );
	image->data.width			= width;
	image->data.height			= height;
	image->data.uv.u0			= 0.0f;
	image->data.uv.v0			= 0.0f;
	image->data.uv.u1			= 1.0f;
	image->data.uv.v1			= 1.0f;
	image->state				= ImAppResState_Ready;

	if( image->data.textureHandle == IMUI_TEXTURE_HANDLE_INVALID )
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
	decodeEvent.type						= ImAppResEventType_DecodePng;
	decodeEvent.data.decode.image			= image;
	decodeEvent.data.decode.sourceData.data	= imageData;
	decodeEvent.data.decode.sourceData.size	= imageDataSize;

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
	decodeEvent.type						= ImAppResEventType_DecodeJpeg;
	decodeEvent.data.decode.image			= image;
	decodeEvent.data.decode.sourceData.data	= imageData;
	decodeEvent.data.decode.sourceData.size	= imageDataSize;

	if( !ImAppResEventQueuePush( ressys, &ressys->sendQueue, &decodeEvent ) )
	{
		ImUiMemoryFree( ressys->allocator, image );
		return NULL;
	}

	return image;
}

ImAppResState ImAppResSysImageGetState( ImAppResSys* ressys, ImAppImage* image )
{
	IMAPP_USE( ressys );

	return image->state;
}

void ImAppResSysImageFree( ImAppResSys* ressys, ImAppImage* image )
{
	if( image->data.textureHandle != IMUI_TEXTURE_HANDLE_INVALID )
	{
		ImAppRendererTextureDestroy( ressys->renderer, (ImAppRendererTexture*)image->data.textureHandle );
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
	uiImage.textureHandle	= (uint64_t)*texture;
	uiImage.width			= width;
	uiImage.height			= height;
	uiImage.uv.u0			= 0.0f;
	uiImage.uv.v0			= 0.0f;
	uiImage.uv.u1			= 1.0f;
	uiImage.uv.v1			= 1.0f;

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
			if( res->data.theme.theme )
			{
				ImUiMemoryFree( ressys->allocator, res->data.theme.theme );
				res->data.theme.theme = NULL;
			}
		}
		break;

	case ImAppResPakType_Blob:
		{
			if( pak->memoryData == NULL )
			{
				ImAppPlatformResourceFree( ressys->platform, res->data.blob.blob );
			}
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

	ImAppResPakHeader header;
	if( pak->memoryData == NULL )
	{
		ImAppFile* file = ImAppPlatformResourceOpen( ressys->platform, pak->resourceName );
		if( !file )
		{
			IMAPP_DEBUG_LOGE( "Failed to open '%s' ResPak.", pak->resourceName );
			return;
		}

		if( ImAppPlatformResourceRead( file, &header, sizeof( header ), 0u ) != sizeof( header ) )
		{
			IMAPP_DEBUG_LOGE( "Failed to read ResPak header." );
			ImAppPlatformResourceClose( ressys->platform, file );
			return;
		}

		if( memcmp( header.magic, IMAPP_RES_PAK_MAGIC, sizeof( header.magic ) ) != 0 )
		{
			char magicBuffer[ 5u ] = { '\0', '\0', '\0', '\0', '\0' };
			memcpy( magicBuffer, &header.magic, sizeof( header.magic ) );
			IMAPP_DEBUG_LOGE( "Invalid ResPak file. Magic doesn't match. Got: '%s', Expected: '%s'", magicBuffer, IMAPP_RES_PAK_MAGIC );
			return;
		}

		pak->metadataSize	= header.resourcesOffset;
		void* metadata		= (byte*)ImUiMemoryAlloc( ressys->allocator, pak->metadataSize );
		pak->metadata		= metadata;

		if( !metadata )
		{
			IMAPP_DEBUG_LOGE( "Failed to allocate ResPak metadata." );
			return;
		}

		const uintsize metadataReadResult = ImAppPlatformResourceRead( file, metadata, pak->metadataSize, 0u );
		ImAppPlatformResourceClose( ressys->platform, file );

		if( metadataReadResult != pak->metadataSize )
		{
			IMAPP_DEBUG_LOGE( "Failed to read ResPak metadata." );
			return;
		}
	}
	else
	{
		header = *(const ImAppResPakHeader*)pak->memoryData;

		if( memcmp( header.magic, IMAPP_RES_PAK_MAGIC, sizeof( header.magic ) ) != 0 )
		{
			char magicBuffer[ 5u ] = { '\0', '\0', '\0', '\0', '\0' };
			memcpy( magicBuffer, &header.magic, sizeof( header.magic ) );
			IMAPP_DEBUG_LOGE( "Invalid ResPak file. Magic doesn't match. Got: '%s', Expected: '%s'", magicBuffer, IMAPP_RES_PAK_MAGIC );
			return;
		}

		pak->metadataSize	= header.resourcesOffset;
		pak->metadata		= pak->memoryData;
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

	ImAppBlob resData;
	if( pak->memoryData == NULL )
	{
		resData = ImAppPlatformResourceLoadRange( ressys->platform, pak->resourceName, sourceRes->dataOffset, sourceRes->dataSize );
		if( !resData.data )
		{
#if IMAPP_ENABLED( IMAPP_DEBUG )
			const ImUiStringView resName = ImAppResPakResourceGetName( pak->metadata, sourceRes );
#else
			//const ImUiStringView resName = ImUiStringViewCreate( "no name" );
#endif
			IMAPP_DEBUG_LOGE( "Failed to load data of resource '%s' in pak '%s'.", resName.data, pak->resourceName );
			return;
		}
	}
	else
	{
		if( sourceRes->dataOffset + sourceRes->dataSize > pak->memoryDataSize )
		{
#if IMAPP_ENABLED( IMAPP_DEBUG )
			const ImUiStringView resName = ImAppResPakResourceGetName( pak->metadata, sourceRes );
#else
			//const ImUiStringView resName = ImUiStringViewCreate( "no name" );
#endif
			IMAPP_DEBUG_LOGE( "Failed to get data of resource '%s' in pak '%s'.", resName.data, pak->resourceName );
			return;
		}

		resData.data	= pak->memoryData + sourceRes->dataOffset;
		resData.size	= sourceRes->dataSize;
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

	resEvent->result.image.width		= (uint16)ihdr.width;
	resEvent->result.image.height		= (uint16)ihdr.height;
	resEvent->result.image.data.data	= pixelData;
	resEvent->result.image.data.size	= pixelDataSize;

	resEvent->success = true;
}

static void ImAppResThreadHandleDecodeJpeg( ImAppResSys* ressys, ImAppResEvent* resEvent )
{
	IMAPP_USE( ressys );
	IMAPP_USE( resEvent );
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

	return ImUiHashMix( ImUiHashString( res->key.name ), hash );
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

	return ImUiHashString( image->resourceName );
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

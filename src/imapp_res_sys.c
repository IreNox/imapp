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

struct ImappResSys
{
	ImuiAllocator*		allocator;
	ImappPlatform*		platform;
	ImappRenderer*		renderer;
	ImuiContext*		imui;
	ImappFileWatcher*	watcher;

	ImappResPak*		firstResPak;

	ImuiHashMap			nameMap;
	ImappRes*			firstUnusedRes;

	ImuiHashMap			imageMap;
	ImappFont*			firstFont;

	ImappThread*		thread;

	ImappResEventQueue	sendQueue;
	ImappResEventQueue	receiveQueue;
};

static void			imappResSysCloseInternal( ImappResSys* ressys, ImappResPak* pak );

static void			imappResSysHandleOpenResPak( ImappResSys* ressys, ImappResEvent* resEvent );
static void			imappResSysHandleLoadResData( ImappResSys* ressys, ImappResEvent* resEvent );
static void			imappResSysHandleImage( ImappResSys* ressys, ImappResEvent* resEvent );

static ImappRes*	imappResSysLoad( ImappResPak* pak, uint16 resIndex );
static void			imappResSysUnload( ImappResPak* pak, ImappRes* res );

static bool			imappResSysFontInitialize( ImappResSys* ressys, ImappFont* font );

static void			imappResThreadEntry( void* arg );
static void			imappResThreadHandleOpenResPak( ImappResSys* ressys, ImappResEvent* resEvent );
static void			imappResThreadHandleLoadResData( ImappResSys* ressys, ImappResEvent* resEvent );
static void			imappResThreadHandleImageLoad( ImappResSys* ressys, ImappResEvent* resEvent );
static void			imappResThreadHandleDecodePng( ImappResSys* ressys, ImappResEvent* resEvent );
static void			imappResThreadHandleDecodeJpeg( ImappResSys* ressys, ImappResEvent* resEvent );

static bool			imappResEventQueueConstruct( ImappResSys* ressys, ImappResEventQueue* queue, uintsize initSize );
static void			imappResEventQueueDestruct( ImappResSys* ressys, ImappResEventQueue* queue );
static bool			imappResEventQueuePush( ImappResSys* ressys, ImappResEventQueue* queue, const ImappResEvent* resEvent );
static bool			imappResEventQueuePop( ImappResEventQueue* queue, ImappResEvent* outEvent, bool wait );

static ImuiHash		imappResSysNameMapHash( const void* key );
static bool			imappResSysNameMapIsKeyEquals( const void* lhs, const void* rhs );
static ImuiHash		imappResSysImageMapHash( const void* key );
static bool			imappResSysImageMapIsKeyEquals( const void* lhs, const void* rhs );

static const ImappResPakResource*	imappResPakResourceGet( const void* base, uint16 index );
static ImuiStringView				imappResPakResourceGetName( const void* base, const ImappResPakResource* res );
static const void*					imappResPakResourceGetHeader( const void* base, const ImappResPakResource* res );

ImappResSys* imappResSysCreate( ImuiAllocator* allocator, ImappPlatform* platform, ImappRenderer* renderer, ImuiContext* imui )
{
	ImappResSys* ressys = IMUI_MEMORY_NEW_ZERO( allocator, ImappResSys );

	ressys->allocator	= allocator;
	ressys->platform	= platform;
	ressys->imui		= imui;
	ressys->renderer	= renderer;
	ressys->watcher		= imappPlatformFileWatcherCreate( platform );

	if( !imuiHashMapConstructSize( &ressys->nameMap, allocator, sizeof( ImappRes* ), imappResSysNameMapHash, imappResSysNameMapIsKeyEquals, 64u ) ||
		!imuiHashMapConstructSize( &ressys->imageMap, allocator, sizeof( ImappRes* ), imappResSysImageMapHash, imappResSysImageMapIsKeyEquals, 64u ) ||
		!imappResEventQueueConstruct( ressys, &ressys->sendQueue, 16u ) ||
		!imappResEventQueueConstruct( ressys, &ressys->receiveQueue, 16u ) )
	{
		imappResSysDestroy( ressys );
		return NULL;
	}

	ressys->thread = imappPlatformThreadCreate( platform, "res sys", imappResThreadEntry, ressys );

	return ressys;
}

void imappResSysDestroy( ImappResSys* ressys )
{
	if( ressys->thread )
	{
		const ImappResEvent resEvent = { .type = ImappResEventType_Quit };
		imappResEventQueuePush( ressys, &ressys->sendQueue, &resEvent );

		imappPlatformThreadDestroy( ressys->thread );
		ressys->thread = NULL;
	}

	for( uintsize i = imuiHashMapFindFirstIndex( &ressys->imageMap ); i != IMUI_SIZE_MAX; i = imuiHashMapFindNextIndex( &ressys->imageMap, i ) )
	{
		ImappImage* image = *(ImappImage**)imuiHashMapGetEntry( &ressys->imageMap, i );
		imappResSysImageFree( ressys, image );
	}

	imappResEventQueueDestruct( ressys, &ressys->sendQueue );
	imappResEventQueueDestruct( ressys, &ressys->receiveQueue );

	imuiHashMapDestruct( &ressys->imageMap );
	imuiHashMapDestruct( &ressys->nameMap );

	imappPlatformFileWatcherDestroy( ressys->platform, ressys->watcher );

	imuiMemoryFree( ressys->allocator, ressys );
}

void imappResSysUpdate( ImappResSys* ressys, bool wait )
{
	if( ressys->watcher )
	{
		ImappFileWatchEvent watchEvent;
		if( imappPlatformFileWatcherPopEvent( ressys->watcher, &watchEvent ) )
		{
			ImappResPak* pak;
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

				ImappResPak* newPak = (ImappResPak*)imuiMemoryAllocZero( ressys->allocator, sizeof( ImappResPak ) + nameLength );
				newPak->ressys	= ressys;
				memcpy( newPak->resourceName, pak->resourceName, nameLength );

				ImappResEvent resEvent;
				resEvent.type				= ImappResEventType_OpenResPak;
				resEvent.data.pak.pak		= newPak;
				resEvent.data.pak.reloadPak	= pak;

				if( !imappResEventQueuePush( ressys, &ressys->sendQueue, &resEvent ) )
				{
					imuiMemoryFree( ressys->allocator, newPak );
				}
			}
		}
	}

	{
		ImappResEvent resEvent;
		while( imappResEventQueuePop( &ressys->receiveQueue, &resEvent, wait ) )
		{
			wait = false;
			switch( resEvent.type )
			{
			case ImappResEventType_OpenResPak:
				imappResSysHandleOpenResPak( ressys, &resEvent );
				break;

			case ImappResEventType_LoadResData:
				imappResSysHandleLoadResData( ressys, &resEvent );
				break;

			case ImappResEventType_LoadImage:
			case ImappResEventType_DecodePng:
			case ImappResEventType_DecodeJpeg:
				imappResSysHandleImage( ressys, &resEvent );
				break;

			case ImappResEventType_Quit:
				return;
			}
		}
	}
}

static void imappResSysHandleOpenResPak( ImappResSys* ressys, ImappResEvent* resEvent )
{
	ImappResPak* pak = resEvent->data.pak.pak;

	if( resEvent->data.pak.reloadPak )
	{
		if( !resEvent->success )
		{
			imappResSysCloseInternal( ressys, pak );
			return;
		}

		ImappResPak* reloadPak = resEvent->data.pak.reloadPak;

		pak->prevResPak = reloadPak->prevResPak;
		pak->nextResPak = reloadPak->nextResPak;

		for( uintsize i = 0; i < pak->resourceCount; ++i )
		{
			pak->resources[ i ].key.pak = reloadPak;
		}

		ImappResPak tempPak = *reloadPak;
		*reloadPak = *pak;
		*pak = tempPak;

		imappResSysCloseInternal( ressys, pak );
		pak = reloadPak;
	}
	else
	{
		if( !resEvent->success )
		{
			pak->state = ImappResState_Error;
			return;
		}

		if( ressys->watcher &&
			pak->memoryData == NULL )
		{
			char pakPath[ 1024u ];
			imappPlatformResourceGetPath( ressys->platform, pakPath, IMAPP_ARRAY_COUNT( pakPath ), pak->resourceName );

			imappPlatformFileWatcherAddPath( ressys->watcher, pakPath );
		}
	}

	for( uintsize i = 0; i < pak->resourceCount; ++i )
	{
		const ImappRes* res = &pak->resources[ i ];
		imuiHashMapInsert( &ressys->nameMap, &res );
	}

	pak->state = ImappResState_Ready;
}

static void imappResSysHandleLoadResData( ImappResSys* ressys, ImappResEvent* resEvent )
{
	ImappRes* res = resEvent->data.res.res;

	switch( res->key.type )
	{
	case ImappResPakType_Texture:
		{
			const ImappResPakResource* sourceRes = imappResPakResourceGet( res->key.pak->metadata, res->key.index );
			const ImappResPakTextureHeader* header = (const ImappResPakTextureHeader*)imappResPakResourceGetHeader( res->key.pak->metadata, sourceRes );

			ImappRendererFormat format = ImappRendererFormat_RGBA8;
			switch( header->format )
			{
			case ImappResPakTextureFormat_A8:		format = ImappRendererFormat_R8; break;
			case ImappResPakTextureFormat_PNG24:
			case ImappResPakTextureFormat_JPEG:
			case ImappResPakTextureFormat_RGB8:		format = ImappRendererFormat_RGB8; break;
			case ImappResPakTextureFormat_PNG32:
			case ImappResPakTextureFormat_RGBA8:	format = ImappRendererFormat_RGBA8; break;
			}

			res->data.texture.texture	= imappRendererTextureCreateFromMemory( ressys->renderer, resEvent->result.loadRes.data.data, header->width, header->height, format, header->flags );
			res->data.texture.width		= header->width;
			res->data.texture.height	= header->height;

			if( res->key.pak->memoryData == NULL )
			{
				imappPlatformResourceFree( ressys->platform, resEvent->result.loadRes.data );
			}

			if( !res->data.texture.texture )
			{
				res->state = ImappResState_Error;
				return;
			}
		}
		break;

	case ImappResPakType_Font:
		{
			ImappResFontData* fontData = &res->data.font;

			const ImappResPakResource* sourceRes = imappResPakResourceGet( res->key.pak->metadata, res->key.index );
			const ImappResPakFontHeader* header = (const ImappResPakFontHeader*)imappResPakResourceGetHeader( res->key.pak->metadata, sourceRes );

			ImuiFontParameters parameters;
			parameters.codepoints			= (const ImuiFontCodepoint*)resEvent->result.loadRes.data.data;
			parameters.codepointCount		= header->codepointCount;
			parameters.fontSize				= header->fontSize;
			parameters.image.textureHandle	= (uint64)fontData->textureRes->data.texture.texture;
			parameters.image.width			= fontData->textureRes->data.texture.width;
			parameters.image.height			= fontData->textureRes->data.texture.height;
			parameters.image.uv.u0			= 0.0f;
			parameters.image.uv.v0			= 0.0f;
			parameters.image.uv.u1			= 1.0f;
			parameters.image.uv.v1			= 1.0f;
			parameters.isScalable			= false;
			parameters.lineGap				= header->fontSize;

			res->data.font.font				= imuiFontCreate( ressys->imui, &parameters );

			if( res->key.pak->memoryData == NULL )
			{
				imappPlatformResourceFree( ressys->platform, resEvent->result.loadRes.data );
			}
		}
		break;

	case ImappResPakType_Blob:
		res->data.blob.blob = resEvent->result.loadRes.data;
		break;

	case ImappResPakType_Image:
	case ImappResPakType_Skin:
	case ImappResPakType_Theme:
	case ImappResPakType_MAX:
		// no data
		break;
	}

	res->state = ImappResState_Ready;
}

static void imappResSysHandleImage( ImappResSys* ressys, ImappResEvent* resEvent )
{
	ImappImage* image = resEvent->data.image.image;

	if( !resEvent->success )
	{
		image->state = ImappResState_Error;
		return;
	}

	const ImappResEventResultImageData* result = &resEvent->result.image;
	image->uiImage.textureHandle	= (uint64)imappRendererTextureCreateFromMemory( ressys->renderer, result->data.data, result->width, result->height, result->format, 0u );
	image->uiImage.width			= result->width;
	image->uiImage.height			= result->height;
	image->uiImage.uv.u0			= 0.0f;
	image->uiImage.uv.v0			= 0.0f;
	image->uiImage.uv.u1			= 1.0f;
	image->uiImage.uv.v1			= 1.0f;

	imuiMemoryFree( ressys->allocator, result->data.data );

	image->state = ImappResState_Ready;
}

static ImappRes* imappResSysLoad( ImappResPak* pak, uint16 resIndex )
{
	if( pak->state != ImappResState_Ready )
	{
		return NULL;
	}

	if( resIndex >= pak->resourceCount )
	{
		IMAPP_DEBUG_LOGW( "Could not find resource %d in '%s'", resIndex, pak->resourceName );
		return NULL;
	}

	ImappRes* res = &pak->resources[ resIndex ];
	if( res->state == ImappResState_Ready )
	{
		res->refCount++;
		//imappResSysChangeUsage( pak->ressys, res, imappResUsage_Used );
		return res;
	}

	const ImappResPakResource* sourceRes = imappResPakResourceGet( pak->metadata, res->key.index );
	IMAPP_ASSERT( sourceRes );

	switch( res->key.type )
	{
	case ImappResPakType_Texture:
		{
			ImappResTextureData* textureData = &res->data.texture;

			if( !textureData->loading )
			{
				ImappResEvent resEvent;
				resEvent.type			= ImappResEventType_LoadResData;
				resEvent.data.res.res	= res;

				if( !imappResEventQueuePush( pak->ressys, &pak->ressys->sendQueue, &resEvent ) )
				{
					res->state = ImappResState_Error;
					return NULL;
				}

				textureData->loading = true;
			}

			return res;
		}
		break;

	case ImappResPakType_Image:
		{
			ImappResImageData* imageData = &res->data.image;

			if( !imageData->textureRes )
			{
				ImappRes* textureRes = imappResSysLoad( pak, sourceRes->textureIndex );
				if( !textureRes )
				{
					return res;
				}

				imageData->textureRes = textureRes;
			}

			if( imageData->textureRes->state == ImappResState_Error )
			{
				res->state = ImappResState_Error;
				return NULL;
			}
			else if( imageData->textureRes->state != ImappResState_Ready )
			{
				return res;
			}

			const ImappResPakImageHeader* header = (const ImappResPakImageHeader*)imappResPakResourceGetHeader( pak->metadata, sourceRes );

			imageData->image.textureHandle	= (uint64)imageData->textureRes->data.texture.texture;
			imageData->image.width			= header->width;
			imageData->image.height			= header->height;
			imageData->image.uv.u0			= header->x / (float)imageData->textureRes->data.texture.width;
			imageData->image.uv.v0			= header->y / (float)imageData->textureRes->data.texture.height;
			imageData->image.uv.u1			= (header->x + header->width) / (float)imageData->textureRes->data.texture.width;
			imageData->image.uv.v1			= (header->y + header->height) / (float)imageData->textureRes->data.texture.height;
		}
		break;

	case ImappResPakType_Skin:
		{
			ImappResSkinData* skinData = &res->data.skin;

			if( sourceRes->textureIndex != IMAPP_RES_PAK_INVALID_INDEX )
			{
				if( !skinData->textureRes )
				{
					ImappRes* textureRes = imappResSysLoad( pak, sourceRes->textureIndex );
					if( !textureRes )
					{
						return res;
					}

					skinData->textureRes = textureRes;
				}

				if( skinData->textureRes->state == ImappResState_Error )
				{
					res->state = ImappResState_Error;
					return NULL;
				}
				else if( skinData->textureRes->state != ImappResState_Ready )
				{
					return res;
				}

				const ImappResPakSkinHeader* header = (const ImappResPakSkinHeader*)imappResPakResourceGetHeader( pak->metadata, sourceRes );

				skinData->skin.textureHandle	= (uint64)skinData->textureRes->data.texture.texture;
				skinData->skin.width			= header->width;
				skinData->skin.height			= header->height;
				skinData->skin.uv.u0			= header->x / (float)skinData->textureRes->data.texture.width;
				skinData->skin.uv.v0			= header->y / (float)skinData->textureRes->data.texture.height;
				skinData->skin.uv.u1			= (header->x + header->width) / (float)skinData->textureRes->data.texture.width;
				skinData->skin.uv.v1			= (header->y + header->height) / (float)skinData->textureRes->data.texture.height;
				skinData->skin.border			= imuiBorderCreate( header->top, header->left, header->bottom, header->right );
			}
		}
		break;

	case ImappResPakType_Font:
		{
			ImappResFontData* fontData = &res->data.font;

			if( sourceRes->textureIndex == IMAPP_RES_PAK_INVALID_INDEX )
			{
				res->state = ImappResState_Error;
				return NULL;
			}

			if( !fontData->textureRes )
			{
				ImappRes* textureRes = imappResSysLoad( pak, sourceRes->textureIndex );
				if( !textureRes )
				{
					return res;
				}

				fontData->textureRes = textureRes;
			}

			if( fontData->textureRes->state == ImappResState_Error )
			{
				res->state = ImappResState_Error;
				return NULL;
			}
			else if( fontData->textureRes->state != ImappResState_Ready )
			{
				return res;
			}

			if( !fontData->loading )
			{
				ImappResEvent resEvent;
				resEvent.type			= ImappResEventType_LoadResData;
				resEvent.data.res.res	= res;

				if( !imappResEventQueuePush( pak->ressys, &pak->ressys->sendQueue, &resEvent ) )
				{
					res->state = ImappResState_Error;
					return NULL;
				}

				fontData->loading = true;
			}

			return res;
		}
		break;

	case ImappResPakType_Theme:
		{
			ImappResThemeData* themeData = &res->data.theme;

			const ImappResPakThemeHeader* header	= (const ImappResPakThemeHeader*)imappResPakResourceGetHeader( pak->metadata, sourceRes );
			const byte* headerEnd					= (const byte*)header + sourceRes->headerSize;

			const uint16* referencedIndices = (const uint16*)&header[ 1u ];
			if( referencedIndices + header->referencedCount > (const uint16*)headerEnd )
			{
				IMAPP_DEBUG_LOGE( "Theme header too small for references." );
				res->state = ImappResState_Error;
				return NULL;
			}

			bool ready = true;
			for( uintsize i = 0; i < header->referencedCount; ++i )
			{
				ImappRes* refRes = imappResSysLoad( pak, referencedIndices[ i ] );
				if( !refRes )
				{
					ready = false;
					continue;
				}

				ready &= (refRes->state == ImappResState_Ready);
			}

			if( !ready )
			{
				return res;
			}

			ImappTheme* theme = IMUI_MEMORY_NEW_ZERO( pak->ressys->allocator, ImappTheme );
			imuiToolboxThemeFillDefault( &theme->uiTheme, NULL );
			imappWindowThemeFillDefault( &theme->windowTheme );

			const ImuiToolboxThemeReflection themeReflection = imuiToolboxThemeReflectionGet();
			const ImuiToolboxThemeReflection windowReflection = imappWindowThemeReflectionGet();

			const ImappResPakThemeField* fields = (const ImappResPakThemeField*)&referencedIndices[ header->referencedCount ];
			if( fields + header->themeFieldCount > (const ImappResPakThemeField*)headerEnd )
			{
				IMAPP_DEBUG_LOGE( "Theme header too small for fields." );
				res->state = ImappResState_Error;
				return NULL;
			}

			const byte* fieldData = (const byte*)&fields[ header->themeFieldCount ];
			for( uintsize fieldIndex = 0u; fieldIndex < header->themeFieldCount; ++fieldIndex )
			{
				const ImappResPakThemeField* field = &fields[ fieldIndex ];

				uintsize fieldSize = 0u;
				switch( field->type )
				{
				case ImuiToolboxThemeReflectionType_Color:	fieldSize = sizeof( ImuiColor ); break;
				case ImuiToolboxThemeReflectionType_Skin:	fieldSize = sizeof( uint16 ); break;
				case ImuiToolboxThemeReflectionType_Image:	fieldSize = sizeof( uint16 ); break;
				case ImuiToolboxThemeReflectionType_Font:	fieldSize = sizeof( uint16 ); break;
				case ImuiToolboxThemeReflectionType_Size:	fieldSize = sizeof( ImuiSize ); break;
				case ImuiToolboxThemeReflectionType_Border:	fieldSize = sizeof( ImuiBorder ); break;
				case ImuiToolboxThemeReflectionType_Float:	fieldSize = sizeof( float ); break;
				case ImuiToolboxThemeReflectionType_Double:	fieldSize = sizeof( double ); break;
				case ImuiToolboxThemeReflectionType_UInt32:	fieldSize = sizeof( uint32 ); break;
				}

				if( fieldData + fieldSize > headerEnd )
				{
					IMAPP_DEBUG_LOGE( "Theme header too small for field data 0x%08x.", field->nameHash );
					res->state = ImappResState_Error;
					return NULL;
				}

				byte* targetBase = NULL;
				byte* targetBaseEnd = NULL;
				const ImuiToolboxThemeReflection* reflection = NULL;
				if( field->base == ImappResPakThemeFieldBase_UiTheme )
				{
					targetBase		= (byte*)&theme->uiTheme;
					targetBaseEnd	= targetBase + sizeof( theme->uiTheme );
					reflection		= &themeReflection;
				}
				else if( field->base == ImappResPakThemeFieldBase_WindowTheme )
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

				const ImuiToolboxThemeReflectionField* reflectionField = NULL;
				for( uintsize reflectionIndex = 0u; reflectionIndex < reflection->count; ++reflectionIndex )
				{
					const ImuiHash nameHash = imuiHashCreate( reflection->fields[ reflectionIndex ].name, strlen( reflection->fields[ reflectionIndex ].name ) );
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
				case ImuiToolboxThemeReflectionType_Color:
					*(ImuiColor*)targetData = *(const ImuiColor*)fieldData;
					break;

				case ImuiToolboxThemeReflectionType_Skin:
				case ImuiToolboxThemeReflectionType_Image:
				case ImuiToolboxThemeReflectionType_Font:
					{
						const uint16 fieldResIndex = *(const uint16*)fieldData;
						if( fieldResIndex == IMAPP_RES_PAK_INVALID_INDEX )
						{
							fieldData += fieldSize;
							continue;
						}

						ImappResPakType fieldResType = ImappResPakType_MAX;
						switch( field->type )
						{
						case ImuiToolboxThemeReflectionType_Skin:	fieldResType = ImappResPakType_Skin; break;
						case ImuiToolboxThemeReflectionType_Image:	fieldResType = ImappResPakType_Image; break;
						case ImuiToolboxThemeReflectionType_Font:	fieldResType = ImappResPakType_Font; break;
						}

						ImappRes* fieldRes = imappResSysLoad( pak, fieldResIndex );
						if( !fieldRes ||
							fieldRes->key.type != fieldResType ||
							fieldRes->state != ImappResState_Ready )
						{
							IMAPP_DEBUG_LOGE( "Theme resource %d for field '%s' is in the wrong state.", fieldResIndex, reflectionField->name );
							res->state = ImappResState_Error;
							return NULL;
						}

						switch( field->type )
						{
						case ImuiToolboxThemeReflectionType_Skin:
							*(ImuiSkin*)targetData = fieldRes->data.skin.skin;
							break;

						case ImuiToolboxThemeReflectionType_Image:
							*(ImuiImage*)targetData = fieldRes->data.image.image;
							break;

						case ImuiToolboxThemeReflectionType_Font:
							*(ImuiFont**)targetData = fieldRes->data.font.font;
							break;
						}
					}
					break;

				case ImuiToolboxThemeReflectionType_Size:
					*(ImuiSize*)targetData = *(const ImuiSize*)fieldData;
					break;

				case ImuiToolboxThemeReflectionType_Border:
					*(ImuiBorder*)targetData = *(const ImuiBorder*)fieldData;
					break;

				case ImuiToolboxThemeReflectionType_Float:
					*(float*)targetData = *(const float*)fieldData;
					break;

				case ImuiToolboxThemeReflectionType_Double:
					*(double*)targetData = *(const double*)fieldData;
					break;

				case ImuiToolboxThemeReflectionType_UInt32:
					*(uint32*)targetData = *(const uint32*)fieldData;
					break;
				}

				fieldData += fieldSize;
			}

			themeData->theme = theme;
		}
		break;

	case ImappResPakType_Blob:
		// TODO
		break;

	case ImappResPakType_MAX:
		break;
	}

	res->refCount++;
	res->state = ImappResState_Ready;
	//imappResSysChangeUsage( pak->ressys, res, imappResUsage_Used );
	return res;
}

void imappResSysDestroyDeviceResources( ImappResSys* ressys )
{
	for( ImappResPak* pak = ressys->firstResPak; pak != NULL; pak = pak->nextResPak )
	{
		for( uintsize i = 0; i < pak->resourceCount; ++i )
		{
			ImappRes* res = &pak->resources[ i ];
			imappResSysUnload( pak, res );
		}
	}

	for( ImappFont* font = ressys->firstFont; font != NULL; font = font->nextFont )
	{
		imappRendererTextureDestroyData( ressys->renderer, font->texture );
	}
}

void imappResSysCreateDeviceResources( ImappResSys* ressys )
{
	for( ImappFont* font = ressys->firstFont; font != NULL; font = font->nextFont )
	{
		imappResSysFontInitialize( ressys, font );
	}
}

ImappResPak* imappResSysAdd( ImappResSys* ressys, const void* pakData, uintsize dataLength )
{
	ImappResPak* pak = (ImappResPak*)imuiMemoryAllocZero( ressys->allocator, sizeof( ImappResPak ) );
	pak->ressys			= ressys;
	pak->memoryData		= (const byte*)pakData;
	pak->memoryDataSize	= dataLength;

	ImappResEvent resEvent;
	resEvent.type				= ImappResEventType_OpenResPak;
	resEvent.data.pak.pak		= pak;
	resEvent.data.pak.reloadPak	= NULL;

	if( !imappResEventQueuePush( ressys, &ressys->sendQueue, &resEvent ) )
	{
		imuiMemoryFree( ressys->allocator, pak );
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

ImappResPak* imappResSysOpen( ImappResSys* ressys, const char* resourceName )
{
	const uintsize nameLength = strlen( resourceName );

	ImappResPak* pak = (ImappResPak*)imuiMemoryAllocZero( ressys->allocator, sizeof( ImappResPak ) + nameLength );
	pak->ressys	= ressys;
	memcpy( pak->resourceName, resourceName, nameLength );

	ImappResEvent resEvent;
	resEvent.type				= ImappResEventType_OpenResPak;
	resEvent.data.pak.pak		= pak;
	resEvent.data.pak.reloadPak	= NULL;

	if( !imappResEventQueuePush( ressys, &ressys->sendQueue, &resEvent ) )
	{
		imuiMemoryFree( ressys->allocator, pak );
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

static void imappResSysCloseInternal( ImappResSys* ressys, ImappResPak* pak )
{
	for( uintsize i = 0; i < pak->resourceCount; ++i )
	{
		ImappRes* res = &pak->resources[ i ];
		imappResSysUnload( pak, res );
		imuiHashMapRemove( &ressys->nameMap, &res );
	}

	if( pak->memoryData == NULL )
	{
		ImappBlob metadataBlob;
		metadataBlob.data = pak->metadata;
		metadataBlob.size = pak->metadataSize;
		imappPlatformResourceFree( ressys->platform, metadataBlob );
	}

	imuiMemoryFree( ressys->allocator, pak->resources );
	imuiMemoryFree( ressys->allocator, pak );

	ImuiToolboxTheme config;
	imuiToolboxThemeFillDefault( &config, NULL );
	imuiToolboxThemeSet( &config );
}

void imappResSysClose( ImappResSys* ressys, ImappResPak* pak )
{
	if( ressys->watcher )
	{
		char pakPath[ 1024u ];
		imappPlatformResourceGetPath( ressys->platform, pakPath, IMAPP_ARRAY_COUNT( pakPath ), pak->resourceName );

		imappPlatformFileWatcherRemovePath( ressys->watcher, pakPath );
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

	imappResSysCloseInternal( ressys, pak );
}

ImappResState imappResPakGetState( const ImappResPak* pak )
{
	return pak->state;
}

ImappResState imappResPakLoadResourceIndex( ImappResPak* pak, uint16_t resIndex )
{
	if( resIndex >= pak->resourceCount )
	{
		return false;
	}

	const ImappRes* res = imappResSysLoad( pak, resIndex );
	if( !res )
	{
		return ImappResState_Error;
	}

	return res->state;
}

ImappResState imappResPakLoadResourceName( ImappResPak* pak, ImappResPakType type, const char* name )
{
	const uint16 resIndex = imappResPakFindResourceIndex( pak, type, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return ImappResState_Error;
	}

	return imappResPakLoadResourceIndex( pak, resIndex );
}

uint16_t imappResPakFindResourceIndex( const ImappResPak* pak, ImappResPakType type, const char* name )
{
	if( !pak || pak->state != ImappResState_Ready )
	{
		return IMAPP_RES_PAK_INVALID_INDEX;
	}

	ImappResKey key;
	key.name	= imuiStringViewCreate( name );
	key.type	= type;
	key.pak		= (ImappResPak*)pak;

	ImappResKey* keyEntry = &key;
	const ImappRes** resKey = (const ImappRes**)imuiHashMapFind( &pak->ressys->nameMap, &keyEntry );
	if( resKey )
	{
		return (*resKey)->key.index;
	}

	return IMAPP_RES_PAK_INVALID_INDEX;
}

const ImuiImage* imappResPakGetImage( ImappResPak* pak, const char* name )
{
	const uint16 resIndex = imappResPakFindResourceIndex( pak, ImappResPakType_Image, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return NULL;
	}

	return imappResPakGetImageIndex( pak, resIndex );
}

const ImuiImage* imappResPakGetImageIndex( ImappResPak* pak, uint16_t resIndex )
{
	ImappRes* res = imappResSysLoad( pak, resIndex );
	if( !res || res->state != ImappResState_Ready )
	{
		return NULL;
	}

	IMAPP_ASSERT( res->key.type == ImappResPakType_Image );
	return &res->data.image.image;
}

const ImuiSkin* imappResPakGetSkin( ImappResPak* pak, const char* name )
{
	const uint16 resIndex = imappResPakFindResourceIndex( pak, ImappResPakType_Skin, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return NULL;
	}

	return imappResPakGetSkinIndex( pak, resIndex );
}

const ImuiSkin* imappResPakGetSkinIndex( ImappResPak* pak, uint16_t resIndex )
{
	ImappRes* res = imappResSysLoad( pak, resIndex );
	if( !res || res->state != ImappResState_Ready )
	{
		return NULL;
	}

	IMAPP_ASSERT( res->key.type == ImappResPakType_Skin );
	return &res->data.skin.skin;
}

ImuiFont* imappResPakGetFont( ImappResPak* pak, const char* name )
{
	const uint16 resIndex = imappResPakFindResourceIndex( pak, ImappResPakType_Font, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return NULL;
	}

	return imappResPakGetFontIndex( pak, resIndex );
}

ImuiFont* imappResPakGetFontIndex( ImappResPak* pak, uint16_t resIndex )
{
	ImappRes* res = imappResSysLoad( pak, resIndex );
	if( !res || res->state != ImappResState_Ready )
	{
		return NULL;
	}

	IMAPP_ASSERT( res->key.type == ImappResPakType_Font );
	return res->data.font.font;
}

const ImappTheme* imappResPakGetTheme( ImappResPak* pak, const char* name )
{
	const uint16 resIndex = imappResPakFindResourceIndex( pak, ImappResPakType_Theme, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		return NULL;
	}

	return imappResPakGetThemeIndex( pak, resIndex );
}

const ImappTheme* imappResPakGetThemeIndex( ImappResPak* pak, uint16_t resIndex )
{
	ImappRes* res = imappResSysLoad( pak, resIndex );
	if( !res || res->state != ImappResState_Ready )
	{
		return NULL;
	}

	IMAPP_ASSERT( res->key.type == ImappResPakType_Theme );
	return res->data.theme.theme;
}

ImappBlob imappResPakGetBlob( ImappResPak* pak, const char* name )
{
	const uint16 resIndex = imappResPakFindResourceIndex( pak, ImappResPakType_Blob, name );
	if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
	{
		const ImappBlob result = { NULL, 0u };
		return result;
	}

	return imappResPakGetBlobIndex( pak, resIndex );
}

ImappBlob imappResPakGetBlobIndex( ImappResPak* pak, uint16_t resIndex )
{
	ImappRes* res = imappResSysLoad( pak, resIndex );
	if( !res || res->state != ImappResState_Ready )
	{
		const ImappBlob result = { NULL, 0u };
		return  result;
	}

	IMAPP_ASSERT( res->key.type == ImappResPakType_Blob );
	return res->data.blob.blob;
}

ImappImage* imappResSysImageCreateRaw( ImappResSys* ressys, const void* pixelData, int width, int height )
{
	ImappImage* image = IMUI_MEMORY_NEW( ressys->allocator, ImappImage );
	image->resourceName				= imuiStringViewCreateEmpty();
	image->uiImage.textureHandle	= (uint64)imappRendererTextureCreateFromMemory( ressys->renderer, pixelData, (uint32)width, (uint32)height, ImappRendererFormat_RGBA8, 0u );
	image->uiImage.width			= (uint32)width;
	image->uiImage.height			= (uint32)height;
	image->uiImage.uv.u0			= 0.0f;
	image->uiImage.uv.v0			= 0.0f;
	image->uiImage.uv.u1			= 1.0f;
	image->uiImage.uv.v1			= 1.0f;
	image->state					= ImappResState_Ready;

	if( image->uiImage.textureHandle == IMUI_TEXTURE_HANDLE_INVALID )
	{
		imuiMemoryFree( ressys->allocator, image );
		return NULL;
	}

	return image;
}

ImappImage* imappResSysImageLoadResource( ImappResSys* ressys, const char* resourceName )
{
	const ImuiStringView resourceNameView = imuiStringViewCreate( resourceName );
	const ImuiStringView* resourceNameViewEntry = &resourceNameView;

	bool isNew;
	ImappImage** imageEntry = (ImappImage**)imuiHashMapInsertNew( &ressys->imageMap, &resourceNameViewEntry, &isNew );
	if( isNew )
	{
		ImappImage* image = (ImappImage*)imuiMemoryAllocZero( ressys->allocator, sizeof( *image ) + resourceNameView.length + 1u );

		char* targetResourceName = (char*)&image[ 1u ];
		memcpy( targetResourceName, resourceName, resourceNameView.length );
		targetResourceName[ resourceNameView.length ] = '\0';

		image->resourceName.data	= targetResourceName;
		image->resourceName.length	= resourceNameView.length;

		ImappResEvent loadEvent;
		loadEvent.type				= ImappResEventType_LoadImage;
		loadEvent.data.image.image	= image;

		if( !imappResEventQueuePush( ressys, &ressys->sendQueue, &loadEvent ) )
		{
			imuiHashMapRemove( &ressys->imageMap, &image );
			imuiMemoryFree( ressys->allocator, image );
			return NULL;
		}

		*imageEntry = image;
	}

	return *imageEntry;
}

ImappImage* imappResSysImageCreatePng( ImappResSys* ressys, const void* imageData, uintsize imageDataSize )
{
	ImappImage* image = IMUI_MEMORY_NEW_ZERO( ressys->allocator, ImappImage );

	void* imageDataCopy = imuiMemoryAlloc( ressys->allocator, imageDataSize );
	if( !imageDataCopy )
	{
		imuiMemoryFree( ressys->allocator, image );
		return NULL;
	}

	memcpy( imageDataCopy, imageData, imageDataSize );

	ImappResEvent decodeEvent;
	decodeEvent.type						= ImappResEventType_DecodePng;
	decodeEvent.data.decode.image			= image;
	decodeEvent.data.decode.sourceData.data	= imageDataCopy;
	decodeEvent.data.decode.sourceData.size	= imageDataSize;

	if( !imappResEventQueuePush( ressys, &ressys->sendQueue, &decodeEvent ) )
	{
		imuiMemoryFree( ressys->allocator, image );
		return NULL;
	}

	return image;
}

ImappImage* imappResSysImageCreateJpeg( ImappResSys* ressys, const void* imageData, uintsize imageDataSize )
{
	ImappImage* image = IMUI_MEMORY_NEW_ZERO( ressys->allocator, ImappImage );

	ImappResEvent decodeEvent;
	decodeEvent.type						= ImappResEventType_DecodeJpeg;
	decodeEvent.data.decode.image			= image;
	decodeEvent.data.decode.sourceData.data	= imageData;
	decodeEvent.data.decode.sourceData.size	= imageDataSize;

	if( !imappResEventQueuePush( ressys, &ressys->sendQueue, &decodeEvent ) )
	{
		imuiMemoryFree( ressys->allocator, image );
		return NULL;
	}

	return image;
}

ImappResState imappResSysImageGetState( ImappResSys* ressys, ImappImage* image )
{
	IMAPP_USE( ressys );

	return image->state;
}

void imappResSysImageFree( ImappResSys* ressys, ImappImage* image )
{
	if( image->uiImage.textureHandle != IMUI_TEXTURE_HANDLE_INVALID )
	{
		imappRendererTextureDestroy( ressys->renderer, (ImappRendererTexture*)image->uiImage.textureHandle );
	}

	imuiMemoryFree( ressys->allocator, image );
}

ImappFont* imappResSysFontCreateSystem( ImappResSys* ressys, const char* fontName, float fontSize )
{
	const uintsize fontNameLength = strlen( fontName );
	ImappFont* font = (ImappFont*)imuiMemoryAllocZero( ressys->allocator, sizeof( ImappFont ) + fontNameLength + 1 );
	if( !font )
	{
		return NULL;
	}

	memcpy( font + 1, fontName, fontNameLength );

	font->name.data		= (const char*)&font[ 1 ];
	font->name.length	= fontNameLength;
	font->size			= fontSize;
	font->texture		= imappRendererTextureCreate( ressys->renderer );

	if( !font->texture )
	{
		imuiMemoryFree( ressys->allocator, font );
		return NULL;
	}

	if( !imappResSysFontInitialize( ressys, font ) )
	{
		imappRendererTextureDestroy( ressys->renderer, font->texture );
		imuiMemoryFree( ressys->allocator, font );
		return NULL;
	}

	font->nextFont = ressys->firstFont;

	if( ressys->firstFont )
	{
		ressys->firstFont->prevFont = font;
	}

	ressys->firstFont = font;

	return font;
}

static bool imappResSysFontInitialize( ImappResSys* ressys, ImappFont* font )
{
	const ImappBlob fontBlob = imappPlatformResourceLoadSystemFont( ressys->platform, font->name.data );
	if( fontBlob.data == NULL )
	{
		return false;
	}

	ImuiFontTrueTypeData* ttf = imuiFontTrueTypeDataCreate( ressys->imui, fontBlob.data, fontBlob.size );
	if( !ttf )
	{
		imuiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	imuiFontTrueTypeDataAddCodepointRange( ttf, 0x20, 0x7e );
	imuiFontTrueTypeDataAddCodepointRange( ttf, 0xa0, 0xff );
	imuiFontTrueTypeDataAddCodepointRange( ttf, 0x370, 0x3ff );
	imuiFontTrueTypeDataAddCodepointRange( ttf, 0x1d00, 0x1d7f );
	imuiFontTrueTypeDataAddCodepointRange( ttf, 0xfffd, 0xfffd );

	uint32_t width;
	uint32_t height;
	imuiFontTrueTypeDataCalculateMinTextureSize( ttf, font->size, &width, &height );
	width = (width + 4u - 1u) & (0u - 4u);
	height = (height + 4u - 1u) & (0u - 4u);

	void* textureData = malloc( width * height );
	if( !textureData )
	{
		imuiFontTrueTypeDataDestroy( ttf );
		imuiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	ImuiFontTrueTypeImage* ttfImage = imuiFontTrueTypeDataGenerateTextureData( ttf, font->size, textureData, width * height, width, height );
	if( !ttfImage )
	{
		imuiFontTrueTypeDataDestroy( ttf );
		imuiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	const bool textureOk = imappRendererTextureInitializeDataFromMemory( ressys->renderer, font->texture, textureData, width, height, ImappRendererFormat_R8, ImappResPakTextureFlags_Font );
	free( textureData );

	if( !textureOk )
	{
		imuiFontTrueTypeDataDestroy( ttf );
		imuiMemoryFree( ressys->allocator, fontBlob.data );
		return false;
	}

	ImuiImage uiImage;
	uiImage.textureHandle	= (uint64_t)font->texture;
	uiImage.width			= width;
	uiImage.height			= height;
	uiImage.uv.u0			= 0.0f;
	uiImage.uv.v0			= 0.0f;
	uiImage.uv.u1			= 1.0f;
	uiImage.uv.v1			= 1.0f;

	font->uiFont = imuiFontCreateTrueType( ressys->imui, ttfImage, uiImage );

	imuiFontTrueTypeDataDestroy( ttf );
	imuiMemoryFree( ressys->allocator, fontBlob.data );

	if( !font->uiFont )
	{
		return false;
	}

	return true;
}

void imappResSysFontDestroy( ImappResSys* ressys, ImappFont* font )
{
	if( !font )
	{
		return;
	}

	if( font->prevFont )
	{
		font->prevFont->nextFont = font->nextFont;
	}

	if( font->nextFont )
	{
		font->nextFont->prevFont = font->prevFont;
	}

	if( font == ressys->firstFont )
	{
		ressys->firstFont = font->nextFont;
	}

	imuiFontDestroy( ressys->imui, font->uiFont );
	imappRendererTextureDestroy( ressys->renderer, font->texture );

	imuiMemoryFree( ressys->allocator, font );
}

static void imappResThreadEntry( void* arg )
{
	ImappResSys* ressys = (ImappResSys*)arg;

	bool running = true;
	ImappResEvent resEvent;
	while( running )
	{
		if( !imappResEventQueuePop( &ressys->sendQueue, &resEvent, true ) )
		{
			continue;
		}

		resEvent.success = false;

		switch( resEvent.type )
		{
		case ImappResEventType_OpenResPak:
			imappResThreadHandleOpenResPak( ressys, &resEvent );
			break;

		case ImappResEventType_LoadResData:
			imappResThreadHandleLoadResData( ressys, &resEvent );
			break;

		case ImappResEventType_LoadImage:
			imappResThreadHandleImageLoad( ressys, &resEvent );
			break;

		case ImappResEventType_DecodePng:
			imappResThreadHandleDecodePng( ressys, &resEvent );
			break;

		case ImappResEventType_DecodeJpeg:
			imappResThreadHandleDecodeJpeg( ressys, &resEvent );
			break;

		case ImappResEventType_Quit:
			running = false;
			break;
		}

		imappResEventQueuePush( ressys, &ressys->receiveQueue, &resEvent );
	}
}

static void imappResSysUnload( ImappResPak* pak, ImappRes* res )
{
	ImappResSys* ressys = res->key.pak->ressys;

	switch( res->key.type )
	{
	case ImappResPakType_Texture:
		{
			if( res->data.texture.texture )
			{
				imappRendererTextureDestroy( ressys->renderer, res->data.texture.texture );
				res->data.texture.texture	= NULL;
				res->data.texture.width		= 0u;
				res->data.texture.height	= 0u;
				res->data.texture.loading	= false;
			}
		}
		break;

	case ImappResPakType_Font:
		{
			if( res->data.font.font )
			{
				imuiFontDestroy( ressys->imui, res->data.font.font );
				res->data.font.font = NULL;
			}
		}
		break;

	case ImappResPakType_Theme:
		{
			if( res->data.theme.theme )
			{
				imuiMemoryFree( ressys->allocator, res->data.theme.theme );
				res->data.theme.theme = NULL;
			}
		}
		break;

	case ImappResPakType_Blob:
		{
			if( pak->memoryData == NULL )
			{
				imappPlatformResourceFree( ressys->platform, res->data.blob.blob );
			}
			res->data.blob.blob.data	= NULL;
			res->data.blob.blob.size	= 0u;
		}
		break;

	case ImappResPakType_Image:
	case ImappResPakType_Skin:
	case ImappResPakType_MAX:
		break;
	}

	memset( &res->data, 0, sizeof( res->data ) );
	res->state		= ImappResState_Loading;
	res->refCount	= 0u;
}

static void imappResThreadHandleOpenResPak( ImappResSys* ressys, ImappResEvent* resEvent )
{
	ImappResPak* pak = resEvent->data.pak.pak;

	ImappResPakHeader header;
	if( pak->memoryData == NULL )
	{
		ImappFile* file = imappPlatformResourceOpen( ressys->platform, pak->resourceName );
		if( !file )
		{
			IMAPP_DEBUG_LOGE( "Failed to open '%s' ResPak.", pak->resourceName );
			return;
		}

		if( imappPlatformResourceRead( file, &header, sizeof( header ), 0u ) != sizeof( header ) )
		{
			IMAPP_DEBUG_LOGE( "Failed to read ResPak header." );
			imappPlatformResourceClose( ressys->platform, file );
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
		void* metadata		= (byte*)imuiMemoryAlloc( ressys->allocator, pak->metadataSize );
		pak->metadata		= metadata;

		if( !metadata )
		{
			IMAPP_DEBUG_LOGE( "Failed to allocate ResPak metadata." );
			return;
		}

		const uintsize metadataReadResult = imappPlatformResourceRead( file, metadata, pak->metadataSize, 0u );
		imappPlatformResourceClose( ressys->platform, file );

		if( metadataReadResult != pak->metadataSize )
		{
			IMAPP_DEBUG_LOGE( "Failed to read ResPak metadata." );
			return;
		}
	}
	else
	{
		header = *(const ImappResPakHeader*)pak->memoryData;

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

	pak->resources		= IMUI_MEMORY_ARRAY_NEW_ZERO( ressys->allocator, ImappRes, header.resourceCount );
	pak->resourceCount	= header.resourceCount;

	if( !pak->resources )
	{
		IMAPP_DEBUG_LOGE( "Failed to allocate ResPak resources." );
		return;
	}

	const ImappResPakResource* sourceResources = (const ImappResPakResource*)(pak->metadata + sizeof( ImappResPakHeader ));
	for( uintsize i = 0; i < pak->resourceCount; ++i )
	{
		const ImappResPakResource* sourceRes = &sourceResources[ i ];
		ImappRes* targetRes = &pak->resources[ i ];

		targetRes->key.index	= (uint16)i;
		targetRes->key.name		= imappResPakResourceGetName( pak->metadata, sourceRes );
		targetRes->key.type		= (ImappResPakType)sourceRes->type;
		targetRes->key.pak		= pak;
	}

	resEvent->success = true;
}

static void imappResThreadHandleLoadResData( ImappResSys* ressys, ImappResEvent* resEvent )
{
	ImappRes* res = resEvent->data.res.res;
	ImappResPak* pak = res->key.pak;

	const ImappResPakResource* sourceRes = imappResPakResourceGet( pak->metadata, res->key.index );
	if( !sourceRes )
	{
		IMAPP_DEBUG_LOGE( "Could not find resource %d in pak '%s'.", res->key.index, pak->resourceName );
		return;
	}

	ImappBlob resData;
	if( pak->memoryData == NULL )
	{
		resData = imappPlatformResourceLoadRange( ressys->platform, pak->resourceName, sourceRes->dataOffset, sourceRes->dataSize );
		if( !resData.data )
		{
#if IMAPP_ENABLED( IMAPP_DEBUG )
			const ImuiStringView resName = imappResPakResourceGetName( pak->metadata, sourceRes );
#else
			//const ImuiStringView resName = imuiStringViewCreate( "no name" );
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
			const ImuiStringView resName = imappResPakResourceGetName( pak->metadata, sourceRes );
#else
			//const ImuiStringView resName = imuiStringViewCreate( "no name" );
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

static void imappResThreadHandleImageLoad( ImappResSys* ressys, ImappResEvent* resEvent )
{
	ImappImage* image = resEvent->data.image.image;

	const ImappBlob data = imappPlatformResourceLoad( ressys->platform, image->resourceName.data );
	if( !data.data )
	{
		IMAPP_DEBUG_LOGE( "Failed to load image '%s'.", image->resourceName.data );
		return;
	}

	const bool isPng = data.size > sizeof( s_pngHeader ) && memcmp( data.data, s_pngHeader, sizeof( s_pngHeader ) ) == 0;
	const bool isJpeg = data.size > sizeof( s_jpegHeader ) && memcmp( data.data, s_jpegHeader, sizeof( s_jpegHeader ) ) == 0;
	if( isPng || isJpeg )
	{
		ImappResEvent decodeEvent;
		decodeEvent.type					= isPng ? ImappResEventType_DecodePng : ImappResEventType_DecodeJpeg;
		decodeEvent.data.decode.image		= image;
		decodeEvent.data.decode.sourceData	= data;
		decodeEvent.success					= false;

		if( isPng )
		{
			imappResThreadHandleDecodePng( ressys, &decodeEvent );
		}
		else
		{
			imappResThreadHandleDecodeJpeg( ressys, &decodeEvent );
		}

		imappPlatformResourceFree( ressys->platform, data );

		if( !decodeEvent.success )
		{
			return;
		}

		resEvent->result.image = decodeEvent.result.image;
	}

	resEvent->success = true;
}

static void imappResThreadHandleDecodePng( ImappResSys* ressys, ImappResEvent* resEvent )
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
		resEvent->result.image.format	= ImappRendererFormat_R8;
		break;

	case SPNG_COLOR_TYPE_INDEXED:
	case SPNG_COLOR_TYPE_TRUECOLOR:
		sourceImageFormat				= SPNG_FMT_RGB8;
		resEvent->result.image.format	= ImappRendererFormat_RGB8;
		break;

	case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
		sourceImageFormat				= SPNG_FMT_RGBA8;
		resEvent->result.image.format	= ImappRendererFormat_RGBA8;
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

	void* pixelData = imuiMemoryAlloc( ressys->allocator, pixelDataSize );
	if( !pixelData )
	{
		IMAPP_DEBUG_LOGE( "Failed to allocate PNG pixel data. Size: %d", pixelDataSize );
		spng_ctx_free( spng );
		return;
	}

	const int decodeResult = spng_decode_image( spng, pixelData, pixelDataSize, (int)sourceImageFormat, 0 );
	spng_ctx_free( spng );

	if( decodeResult != 0 )
	{
		IMAPP_DEBUG_LOGE( "Failed to decode PNG. Result: %d", decodeResult );
		imuiMemoryFree( ressys->allocator, pixelData );
		return;
	}

	resEvent->result.image.width		= (uint16)ihdr.width;
	resEvent->result.image.height		= (uint16)ihdr.height;
	resEvent->result.image.data.data	= pixelData;
	resEvent->result.image.data.size	= pixelDataSize;

	resEvent->success = true;
}

static void imappResThreadHandleDecodeJpeg( ImappResSys* ressys, ImappResEvent* resEvent )
{
	IMAPP_USE( ressys );
	IMAPP_USE( resEvent );
	// TODO
}

static bool imappResEventQueueConstruct( ImappResSys* ressys, ImappResEventQueue* queue, uintsize initSize )
{
	queue->semaphore	= imappPlatformSemaphoreCreate( ressys->platform );
	queue->mutex		= imappPlatformMutexCreate( ressys->platform );
	queue->events		= IMUI_MEMORY_ARRAY_NEW( ressys->allocator, ImappResEvent, initSize );
	queue->top			= 0u;
	queue->bottom		= 0u;
	queue->count		= 0u;
	queue->capacity		= initSize;

	if( !queue->semaphore ||
		!queue->mutex ||
		!queue->events )
	{
		imappResEventQueueDestruct( ressys, queue );
		return false;
	}

	return true;
}

static void imappResEventQueueDestruct( ImappResSys* ressys, ImappResEventQueue* queue )
{
	imuiMemoryFree( ressys->allocator, queue->events );
	queue->events	= NULL;
	queue->top		= 0u;
	queue->bottom	= 0u;
	queue->count	= 0u;
	queue->capacity	= 0u;

	if( queue->mutex )
	{
		imappPlatformMutexDestroy( ressys->platform, queue->mutex );
		queue->mutex = NULL;
	}

	if( queue->semaphore )
	{
		imappPlatformSemaphoreDestroy( ressys->platform, queue->semaphore );
		queue->semaphore = NULL;
	}
}

static bool imappResEventQueuePush( ImappResSys* ressys, ImappResEventQueue* queue, const ImappResEvent* resEvent )
{
	imappPlatformMutexLock( queue->mutex );

	if( queue->count + 1u > queue->capacity )
	{
		const uintsize nextCapacity = IMUI_MAX( IMUI_DEFAULT_ARRAY_CAPACITY, queue->capacity << 1u );
		const uintsize newSize		= nextCapacity * sizeof( ImappResEvent );

		ImappResEvent* newEvents = (ImappResEvent*)imuiMemoryAlloc( ressys->allocator, newSize );
		if( !newEvents )
		{
			imappPlatformMutexUnlock( queue->mutex );
			return false;
		}

		for(uintsize i = 0; i < queue->count; ++i )
		{
			const uintsize index = (queue->top + i) % queue->capacity;
			newEvents[ i ] = queue->events[ index ];
		}

		imuiMemoryFree( ressys->allocator, queue->events );

		queue->events	= newEvents;
		queue->top		= 0u;
		queue->bottom	= queue->count;
		queue->capacity	= nextCapacity;
	}

	const uintsize index = queue->bottom;
	queue->bottom = (queue->bottom + 1u) % queue->capacity;
	queue->count++;

	queue->events[ index ] = *resEvent;

	imappPlatformMutexUnlock( queue->mutex );

	imappPlatformSemaphoreInc( queue->semaphore );
	return true;
}

static bool imappResEventQueuePop( ImappResEventQueue* queue, ImappResEvent* outEvent, bool wait )
{
	if( !imappPlatformSemaphoreDec( queue->semaphore, wait ) )
	{
		return false;
	}

	imappPlatformMutexLock( queue->mutex );

	if( queue->count == 0u )
	{
		imappPlatformMutexUnlock( queue->mutex );
		return false;
	}

	*outEvent = queue->events[ queue->top ];

	queue->top = (queue->top + 1u) % queue->capacity;
	queue->count--;

	imappPlatformMutexUnlock( queue->mutex );
	return true;
}

static ImuiHash imappResSysNameMapHash( const void* key )
{
	const ImappRes* res = *(const ImappRes**)key;

	const uintsize pakInt = (uintsize)res->key.pak;
	ImuiHash hash = res->key.type;
	hash ^= pakInt & 0xffffffffu;
#if IMAPP_ENABLED( IMAPP_POINTER_64 )
	hash ^= pakInt >> 32u;
#endif

	return imuiHashMix( imuiHashString( res->key.name ), hash );
}

static bool imappResSysNameMapIsKeyEquals( const void* lhs, const void* rhs )
{
	const ImappRes* lhsRes = *(const ImappRes**)lhs;
	const ImappRes* rhsRes = *(const ImappRes**)rhs;

	return lhsRes->key.pak == rhsRes->key.pak &&
		lhsRes->key.type == rhsRes->key.type &&
		imuiStringViewIsEquals( lhsRes->key.name, rhsRes->key.name );
}

static ImuiHash imappResSysImageMapHash( const void* key )
{
	const ImappImage* image = *(const ImappImage**)key;

	return imuiHashString( image->resourceName );
}

static bool imappResSysImageMapIsKeyEquals( const void* lhs, const void* rhs )
{
	const ImappImage* lhsImage = *(const ImappImage**)lhs;
	const ImappImage* rhsImage = *(const ImappImage**)rhs;

	return imuiStringViewIsEquals( lhsImage->resourceName, rhsImage->resourceName );
}

static const ImappResPakResource* imappResPakResourceGet( const void* base, uint16 index )
{
	const ImappResPakHeader* header	= (const ImappResPakHeader*)base;
	if( index >= header->resourceCount )
	{
		return NULL;
	}

	const byte* bytes = (const byte*)base;
	bytes += sizeof( ImappResPakHeader );
	bytes += sizeof( ImappResPakResource ) * index;

	return (const ImappResPakResource*)bytes;
}

static ImuiStringView imappResPakResourceGetName( const void* base, const ImappResPakResource* res )
{
	const byte* bytes = (const byte*)base;
	bytes += res->nameOffset;

	return imuiStringViewCreateLength( (const char*)bytes, res->nameLength );
}

static const void* imappResPakResourceGetHeader( const void* base, const ImappResPakResource* res )
{
	const byte* bytes = (const byte*)base;
	bytes += res->headerOffset;

	return bytes;
}

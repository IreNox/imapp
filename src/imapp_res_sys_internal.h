#pragma once

#include "imapp/imapp.h"

#include "imapp_types.h"
#include "imapp_res_pak.h"

typedef struct ImappRendererTexture ImappRendererTexture;
typedef struct ImappRes ImappRes;
typedef struct ImappResPak ImappResPak;

typedef enum ImappResEventType
{
	ImappResEventType_OpenResPak,
	ImappResEventType_LoadResData,
	ImappResEventType_LoadImage,
	ImappResEventType_DecodePng,
	ImappResEventType_DecodeJpeg,
	ImappResEventType_Quit
} ImappResEventType;

typedef struct ImappResEventPakData
{
	ImappResPak*			pak;
	ImappResPak*			reloadPak;
} ImappResEventPakData;

typedef struct ImappResEventResData
{
	ImappRes*				res;
} ImappResEventResData;

typedef struct ImappResEventImageData
{
	ImappImage*				image;
} ImappResEventImageData;

typedef struct ImappResEventDecodeData
{
	ImappImage*				image;
	ImappBlob				sourceData;
} ImappResEventDecodeData;

typedef union ImappResEventData
{
	ImappResEventPakData	pak;
	ImappResEventResData	res;
	ImappResEventImageData	image;
	ImappResEventDecodeData	decode;
} ImappResEventData;

typedef struct ImappResEventResultLoadResData
{
	ImappBlob					data;
} ImappResEventResultLoadResData;

typedef struct ImappResEventResultImageData
{
	uint16						width;
	uint16						height;
	ImappRendererFormat			format;
	ImappBlob					data;
} ImappResEventResultImageData;

typedef union ImappResEventResultData
{
	ImappResEventResultLoadResData	loadRes;
	ImappResEventResultImageData	image;
} ImappResEventResultData;

typedef struct ImappResEvent
{
	ImappResEventType		type;
	ImappResEventData		data;
	ImappResEventResultData	result;
	bool					success;
} ImappResEvent;

typedef enum ImappResUsage
{
	ImappResUsage_Invalid,
	ImappResUsage_Used,
	ImappResUsage_Unused
} ImappResUsage;

typedef struct ImappResTextureData
{
	ImappRendererTexture*	texture;
	uint32					width;
	uint32					height;
	bool					loading;
} ImappResTextureData;

typedef struct ImappResImageData
{
	ImappRes*				textureRes;
	ImuiImage				image;
} ImappResImageData;

typedef struct ImappResSkinData
{
	ImappRes*				textureRes;
	ImuiSkin				skin;
} ImappResSkinData;

typedef struct ImappResFontData
{
	ImappRes*				textureRes;
	ImuiFont*				font;
	bool					loading;
} ImappResFontData;

typedef struct ImappResThemeData
{
	ImappTheme*				theme;
} ImappResThemeData;

typedef struct ImappResBlobData
{
	ImappBlob				blob;
} ImappResBlobData;

typedef union ImappResData
{
	ImappResTextureData		texture;
	ImappResImageData		image;
	ImappResSkinData		skin;
	ImappResFontData		font;
	ImappResThemeData		theme;
	ImappResBlobData		blob;
} ImappResData;

typedef struct ImappResKey
{
	uint16					index;
	ImappResPakType			type;
	ImuiStringView			name;
	ImappResPak*			pak;
} ImappResKey;

typedef struct ImappRes
{
	ImappResKey				key;

	ImappRes*				prevUsageRes;
	ImappRes*				nextUsageRes;

	uint32					refCount;
	uint8					usage;			// imappResUsage
	uint8					state;			// imappResState
	uint64					lastUseTime;

	ImappResData			data;
} ImappRes;

struct ImappResPak
{
	ImappResSys*			ressys;

	ImappResPak*			prevResPak;
	ImappResPak*			nextResPak;

	ImappResState			state;

	ImappRes*				resources;
	uintsize				resourceCount;

	const byte*				metadata;
	uintsize				metadataSize;

	const byte*				memoryData;
	uintsize				memoryDataSize;

	char					resourceName[ 1u ];
};

typedef struct ImappResEventQueue
{
	ImappSemaphore*		semaphore;
	ImappMutex*			mutex;
	ImappResEvent*		events;
	uintsize			top;
	uintsize			bottom;
	uintsize			count;
	uintsize			capacity;
} ImappResEventQueue;

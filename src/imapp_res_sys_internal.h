#pragma once

#include "imapp/imapp.h"

#include "imapp_types.h"
#include "imapp_res_pak.h"

typedef struct ImAppRendererTexture ImAppRendererTexture;
typedef struct ImAppRes ImAppRes;
typedef struct ImAppResPak ImAppResPak;

typedef enum ImAppResEventType
{
	ImAppResEventType_OpenResPak,
	ImAppResEventType_LoadResData,
	ImAppResEventType_LoadImage,
	ImAppResEventType_DecodePng,
	ImAppResEventType_DecodeJpeg
} ImAppResEventType;

typedef struct ImAppResEventPakData
{
	ImAppResPak*			pak;
} ImAppResEventPakData;

typedef struct ImAppResEventResData
{
	ImAppRes*				res;
} ImAppResEventResData;

typedef struct ImAppResEventImageData
{
	ImAppImage*				image;
} ImAppResEventImageData;

typedef struct ImAppResEventDecodeData
{
	ImAppImage*				image;
	ImAppBlob				sourceData;
} ImAppResEventDecodeData;

typedef union ImAppResEventData
{
	ImAppResEventPakData	pak;
	ImAppResEventResData	res;
	ImAppResEventImageData	image;
	ImAppResEventDecodeData	decode;
} ImAppResEventData;

typedef struct ImAppResEventResultLoadResData
{
	ImAppBlob					data;
} ImAppResEventResultLoadResData;

typedef struct ImAppResEventResultImageData
{
	uint32						width;
	uint32						height;
	ImAppRendererFormat			format;
	ImAppBlob					data;
} ImAppResEventResultImageData;

typedef union ImAppResEventResultData
{
	ImAppResEventResultLoadResData	loadRes;
	ImAppResEventResultImageData	image;
} ImAppResEventResultData;

typedef struct ImAppResEvent
{
	ImAppResEventType		type;
	ImAppResEventData		data;
	ImAppResEventResultData	result;
	bool					success;
} ImAppResEvent;

typedef enum ImAppResUsage
{
	ImAppResUsage_Invalid,
	ImAppResUsage_Used,
	ImAppResUsage_Unused
} ImAppResUsage;

typedef struct ImAppResTextureData
{
	ImUiTexture				texture;
} ImAppResTextureData;

typedef struct ImAppResImageData
{
	ImAppRes*				textureRes;
	ImUiImage				image;
} ImAppResImageData;

typedef struct ImAppResSkinData
{
	ImAppRes*				textureRes;
	ImUiSkin				skin;
} ImAppResSkinData;

typedef struct ImAppResFontData
{
	ImAppRes*				textureRes;
	ImUiFont*				font;
} ImAppResFontData;

typedef struct ImAppResThemeData
{
	ImAppRes**				refRes;
	uintsize				refResCount;
	ImUiToolboxConfig*		config;
} ImAppResThemeData;

typedef struct ImAppResBlobData
{
	ImAppBlob				blob;
} ImAppResBlobData;

typedef union ImAppResData
{
	ImAppResTextureData		texture;
	ImAppResImageData		image;
	ImAppResSkinData		skin;
	ImAppResFontData		font;
	ImAppResThemeData		theme;
	ImAppResBlobData		blob;
} ImAppResData;

typedef struct ImAppResKey
{
	uint16					index;
	ImAppResPakType			type;
	ImUiStringView			name;
	ImAppResPak*			pak;
} ImAppResKey;

typedef struct ImAppRes
{
	ImAppResKey				key;

	ImAppRes*				prevUsageRes;
	ImAppRes*				nextUsageRes;

	uint32					refCount;
	uint8					usage;			// ImAppResUsage
	uint8					state;			// ImAppResState
	uint64					lastUseTime;

	ImAppResData			data;
} ImAppRes;

struct ImAppResPak
{
	ImAppResSys*			ressys;

	ImAppResPak*			prevResPak;
	ImAppResPak*			nextResPak;

	ImAppResState			state;

	ImAppRes*				resources;
	uintsize				resourceCount;

	byte*					metadata;
	uintsize				metadataSize;

	char					resourceName[ 1u ];
};

typedef struct ImAppResEventQueue
{
	ImAppSemaphore*		semaphore;
	ImAppMutex*			mutex;
	ImAppResEvent*		events;
	uintsize			eventCount;
	uintsize			eventCapacity;
} ImAppResEventQueue;
#pragma once

#include "imapp/imapp.h"

#include "imapp_types.h"

typedef struct ImAppRendererTexture ImAppRendererTexture;
typedef struct ImAppRes ImAppRes;
typedef struct ImAppResPak ImAppResPak;

typedef enum ImAppResEventType
{
	ImAppResEventType_OpenResPak,
	ImAppResEventType_LoadRes,
	ImAppResEventType_LoadImage,
	ImAppResEventType_DecodePng,
	ImAppResEventType_DecodeJpeg,
} ImAppResEventType;

typedef struct ImAppResEventResPakData
{
	ImAppResPak*			respak;
	uint16					resIndex;
} ImAppResEventResPakData;

typedef struct ImAppResEventImageData
{
	ImAppImage*				image;
} ImAppResEventImageData;

typedef struct ImAppResEventDecodeData
{
	ImAppImage*				image;
	const void*				sourceData;
	uintsize				sourceDataSize;
} ImAppResEventDecodeData;

typedef union ImAppResEventData
{
	ImAppResEventResPakData	respak;
	ImAppResEventImageData	image;
	ImAppResEventDecodeData	decode;
} ImAppResEventData;

typedef struct ImAppResEvent
{
	ImAppResEventType		type;
	ImAppResEventData		data;
} ImAppResEvent;

typedef enum ImAppResState
{
	ImAppResState_Invalid,
	ImAppResState_Used,
	ImAppResState_Unused
} ImAppResState;

typedef struct ImAppResTextureData
{
	ImAppRendererTexture*	texture;
} ImAppResTextureData;

typedef struct ImAppResImageData
{
	ImAppRes*				texture;
	ImUiTexture				image;
} ImAppResImageData;

typedef struct ImAppResSkinData
{
	ImAppRes*				texture;
	ImUiSkin				skin;
} ImAppResSkinData;

typedef struct ImAppResFontData
{
	ImAppRes*				texture;
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

typedef struct ImAppRes
{
	ImUiHash				key;
	ImUiStringView			name;
	ImAppResPakType			type;

	ImAppRes*				prevLoadedRes;
	ImAppRes*				nextLoadedRes;
	ImAppRes*				prevUsageRes;
	ImAppRes*				nextUsageRes;

	uint32					refCount;
	ImAppResState			state;
	uint64					lastUseTime;

	ImAppResData			data;
} ImAppRes;

struct ImAppResPak
{
	ImAppResSys*			ressys;

	void*					metadata;
	uintsize				metadataSize;

	ImAppRes*				firstLoadedRes;

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

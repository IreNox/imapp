#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "imapp/imapp.h"

#define IMAPP_RES_PAK_MAGIC "IAR1"

typedef struct ImAppResPakHeader
{
	uint8_t		magic[ 4u ];
	uint16_t	resourceCount;
	uint32_t	resourcesOffset;

	uint32_t	resourcesByTypeIndexOffset[ ImAppResPakType_MAX ];
	uint16_t	resourcesbyTypeCount[ ImAppResPakType_MAX ];
} ImAppResPakHeader;

typedef struct ImAppResPakResource ImAppResPakResource;
struct ImAppResPakResource
{
	uint8_t		type;
	uint8_t		nameLength;
	uint16_t	textureIndex;	// only for: image, text and font
	uint32_t	nameOffset;
	uint32_t	headerOffset;
	uint32_t	headerSize;
	uint32_t	dataOffset;
	uint32_t	dataSize;
};

const uint16_t*				ImAppResPakResourcesByType( const void* base, ImAppResPakType type );
uint16_t					ImAppResPakResourcesByTypeCount( const void* base, ImAppResPakType type );

const ImAppResPakResource*	ImAppResPakResourceGet( const void* base, uint16_t index );
const ImAppResPakResource*	ImAppResPakResourceFind( const void* base, const char* name, ImAppResPakType type );

ImUiStringView				ImAppResPakResourceGetName( const void* base, const ImAppResPakResource* res );
const void*					ImAppResPakResourceGetHeader( const void* base, const ImAppResPakResource* res );
const void*					ImAppResPakResourceGetData( const void* base, const ImAppResPakResource* res );

typedef enum ImAppResPakTextureFormat
{
	ImAppResPakTextureFormat_RGB8,
	ImAppResPakTextureFormat_RGBA8,
	ImAppResPakTextureFormat_PNG24,
	ImAppResPakTextureFormat_PNG32,
	ImAppResPakTextureFormat_JPEG,

	ImAppResPakTextureFormat_MAX
} ImAppResPakTextureFormat;

typedef struct ImAppResPakTextureHeader
{
	uint8_t		format;		// ImAppResPakTextureFormat
	uint8_t		padding0;
	uint16_t	width;
	uint16_t	height;
} ImAppResPakTextureHeader;

typedef struct ImAppResPakImageHeader
{
	uint16_t	x;
	uint16_t	y;
	uint16_t	width;
	uint16_t	height;
} ImAppResPakImageHeader;

typedef struct ImAppResPakSkinHeader
{
	uint16_t	x;
	uint16_t	y;
	uint16_t	width;
	uint16_t	height;
	float		top;
	float		left;
	float		bottom;
	float		right;
} ImAppResPakSkinHeader;

typedef struct ImAppResPakFontHeader
{
	uint32_t	codepointCount;
	uint32_t	ttfDataSize;
} ImAppResPakFontHeader;

typedef struct ImAppResPakThemeHeader
{
	uint16_t						referencesCount;

	ImUiColor						colors[ ImUiToolboxColor_MAX ];
	uint16_t						skinIndices[ ImUiToolboxSkin_MAX ];
	uint16_t						imageIndices[ ImUiToolboxImage_MAX ];
	uint16_t						fontIndex;

	ImUiToolboxButtonConfig			button;
	ImUiToolboxCheckBoxConfig		checkBox;
	ImUiToolboxSliderConfig			slider;
	ImUiToolboxTextEditConfig		textEdit;
	ImUiToolboxProgressBarConfig	progressBar;
	ImUiToolboxScrollAreaConfig		scrollArea;
	ImUiToolboxListConfig			list;
	ImUiToolboxDropDownConfig		dropDown;
	ImUiToolboxPopupConfig			popup;
} ImAppResPakThemeHeader;

#ifdef __cplusplus
}
#endif

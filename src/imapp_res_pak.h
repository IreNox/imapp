#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "imapp/imapp.h"

#define IMAPP_RES_PAK_MAGIC "IAR1"

typedef struct ImappResPakHeader
{
	uint8_t		magic[ 4u ];
	uint16_t	resourceCount;
	uint32_t	resourcesOffset;

	uint32_t	resourcesByTypeIndexOffset[ ImappResPakType_MAX ];
	uint16_t	resourcesbyTypeCount[ ImappResPakType_MAX ];
} ImappResPakHeader;

typedef struct ImappResPakResource
{
	uint8_t		type;
	uint8_t		nameLength;
	uint16_t	textureIndex;	// only for: image and font
	uint32_t	nameOffset;
	uint32_t	headerOffset;
	uint32_t	headerSize;
	uint32_t	dataOffset;
	uint32_t	dataSize;
} ImappResPakResource;

typedef enum ImappResPakTextureFormat
{
	ImappResPakTextureFormat_A8,
	ImappResPakTextureFormat_RGB8,
	ImappResPakTextureFormat_RGBA8,
	ImappResPakTextureFormat_PNG24,
	ImappResPakTextureFormat_PNG32,
	ImappResPakTextureFormat_JPEG,

	ImappResPakTextureFormat_MAX
} ImappResPakTextureFormat;

typedef enum ImappResPakTextureFlags
{
	ImappResPakTextureFlags_Opaque	= 1u << 0u,
	ImappResPakTextureFlags_Font	= 1u << 1u,
	ImappResPakTextureFlags_FontSdf	= 1u << 2u,
	ImappResPakTextureFlags_Repeat	= 1u << 3u
} ImappResPakTextureFlags;


typedef struct ImappResPakTextureHeader
{
	uint8_t		format;		// imappResPakTextureFormat
	uint8_t		flags;
	uint16_t	width;
	uint16_t	height;
} ImappResPakTextureHeader;

typedef struct ImappResPakImageHeader
{
	uint16_t	x;
	uint16_t	y;
	uint16_t	width;
	uint16_t	height;
} ImappResPakImageHeader;

typedef struct ImappResPakSkinHeader
{
	uint16_t	x;
	uint16_t	y;
	uint16_t	width;
	uint16_t	height;
	float		top;
	float		left;
	float		bottom;
	float		right;
} ImappResPakSkinHeader;

typedef struct ImappResPakFontHeader
{
	uint32_t	codepointCount;
	uint32_t	ttfDataSize;

	float		fontSize;
	float		lineGap;
	bool		isScalable;
} ImappResPakFontHeader;

typedef enum ImappResPakThemeFieldBase
{
	ImappResPakThemeFieldBase_UiTheme,
	ImappResPakThemeFieldBase_WindowTheme
} ImappResPakThemeFieldBase;

typedef struct ImappResPakThemeField
{
	uint32_t						nameHash;
	uint16_t						type;		// ImUiToolboxThemeReflectionType
	uint16_t						base;		// imappResPakThemeFieldBase
} ImappResPakThemeField;

typedef struct ImappResPakThemeHeader
{
	uint16_t						referencedCount;
	uint16_t						themeFieldCount;
} ImappResPakThemeHeader;

#ifdef __cplusplus
}
#endif

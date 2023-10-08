#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "imui/imui.h"
#include "imui/imui_toolbox.h"

#include <stdint.h>

typedef struct ImAppResPakHeader ImAppResPakHeader;
struct ImAppResPakHeader
{
	uint8_t		magic[ 4u ];
	uint16_t	resourceCount;
};

typedef enum ImAppResPakType ImAppResPakType;
enum ImAppResPakType
{
	ImAppResPakType_TextureAtlas,
	ImAppResPakType_Image,
	ImAppResPakType_Skin,
	ImAppResPakType_FontInfo,
	ImAppResPakType_FontTtf,
	ImAppResPakType_ToolboxConfig,

	ImAppResPakType_MAX
};

typedef struct ImAppResPakResource ImAppResPakResource;
struct ImAppResPakResource
{
	uint8_t		type;
	uint8_t		nameLength;
	uint16_t	parentIndex;
	uint32_t	nameOffset;
	uint32_t	offset;
	uint32_t	size;
};

ImUiStringView	ImAppResPakResourceGetName( const void* base, ImAppResPakResource res );
const void*		ImAppResPakResourceGetData( const void* base, ImAppResPakResource res );

typedef enum ImAppResPakTextureFormat ImAppResPakTextureFormat;
enum ImAppResPakTextureFormat
{
	ImAppResPakTextureFormat_RGB8,
	ImAppResPakTextureFormat_RGBA8,
	ImAppResPakTextureFormat_PNG24,
	ImAppResPakTextureFormat_PNG32,

	ImAppResPakTextureFormat_MAX
};

typedef struct ImAppResPakTextureAtlasData ImAppResPakTextureAtlasData;
struct ImAppResPakTextureAtlasData
{
	uint8_t		format;
	uint8_t		padding1;
	uint16_t	width;
	uint16_t	height;
};

typedef struct ImAppResPakImageData ImAppResPakImageData;
struct ImAppResPakImageData
{
	uint16_t	atlasIndex;
	uint16_t	x;
	uint16_t	y;
	uint16_t	width;
	uint16_t	height;
};

typedef struct ImAppResPakSkinData ImAppResPakSkinData;
struct ImAppResPakSkinData
{
	uint16_t	atlasIndex;
	uint16_t	x;
	uint16_t	y;
	uint16_t	width;
	uint16_t	height;
	ImUiBorder	border;
};

typedef struct ImAppResPakFontInfoData ImAppResPakFontInfoData;
struct ImAppResPakFontInfoData
{
	uint16_t	ttfIndex;
	uint16_t	atlasIndex;
	uint32_t	codepointCount;
};

typedef struct ImAppResPakToolboxConfigData ImAppResPakToolboxConfigData;
struct ImAppResPakToolboxConfigData
{
	ImUiColor						colors[ ImUiToolboxColor_MAX ];
	uint16_t						skinIndices[ ImUiToolboxSkin_MAX ];
	uint16_t						fontIndex;
	ImUiToolboxButtonConfig			button;
	ImUiToolboxCheckBoxConfig		checkBox;
	ImUiToolboxSliderConfig			slider;
	ImUiToolboxTextEditConfig		textEdit;
	ImUiToolboxProgressBarConfig	progressBar;
};

#ifdef __cplusplus
}
#endif

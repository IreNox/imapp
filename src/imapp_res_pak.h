#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "imui/imui.h"
#include "imui/imui_toolbox.h"

#include <stdint.h>

#define IMAPP_RES_PAK_INVALID_INDEX	0xffffu

typedef enum ImAppResPakType ImAppResPakType;
enum ImAppResPakType
{
	ImAppResPakType_Texture,
	ImAppResPakType_Image,
	ImAppResPakType_Skin,
	ImAppResPakType_Font,
	ImAppResPakType_Theme,

	ImAppResPakType_MAX
};

typedef struct ImAppResPakHeader ImAppResPakHeader;
struct ImAppResPakHeader
{
	uint8_t		magic[ 4u ];
	uint16_t	resourceCount;
	uint32_t	resourcesOffset;

	uint32_t	resourcesByTypeIndexOffset[ ImAppResPakType_MAX ];
	uint16_t	resourcesbyTypeCount[ ImAppResPakType_MAX ];
};

typedef struct ImAppResPakResource ImAppResPakResource;
struct ImAppResPakResource
{
	uint8_t		type;
	uint8_t		nameLength;
	uint16_t	textureIndex;	// only for: image, text and font
	uint32_t	nameOffset;
	uint32_t	offset;
	uint32_t	size;
};

const uint16_t*			ImAppResPakResourcesByType( const void* base, ImAppResPakType type );
uint16_t				ImAppResPakResourcesByTypeCount( const void* base, ImAppResPakType type );

ImAppResPakResource*	ImAppResPakResourceGet( const void* base, uint16_t index );
ImAppResPakResource*	ImAppResPakResourceFind( const void* base, const char* name, ImAppResPakType type );

ImUiStringView			ImAppResPakResourceGetName( const void* base, ImAppResPakResource* res );
const void*				ImAppResPakResourceGetData( const void* base, ImAppResPakResource* res );

typedef enum ImAppResPakTextureFormat ImAppResPakTextureFormat;
enum ImAppResPakTextureFormat
{
	ImAppResPakTextureFormat_RGB8,
	ImAppResPakTextureFormat_RGBA8,
	ImAppResPakTextureFormat_PNG24,
	ImAppResPakTextureFormat_PNG32,
	ImAppResPakTextureFormat_JPEG,

	ImAppResPakTextureFormat_MAX
};

typedef struct ImAppResPakTextureData ImAppResPakTextureData;
struct ImAppResPakTextureData
{
	uint8_t		format;		// ImAppResPakTextureFormat
	uint8_t		padding0;
	uint16_t	width;
	uint16_t	height;
};

typedef struct ImAppResPakImageData ImAppResPakImageData;
struct ImAppResPakImageData
{
	uint16_t	x;
	uint16_t	y;
	uint16_t	width;
	uint16_t	height;
};

typedef struct ImAppResPakSkinData ImAppResPakSkinData;
struct ImAppResPakSkinData
{
	uint16_t	x;
	uint16_t	y;
	uint16_t	width;
	uint16_t	height;
	float		top;
	float		left;
	float		bottom;
	float		right;
};

typedef struct ImAppResPakFontData ImAppResPakFontData;
struct ImAppResPakFontData
{
	uint32_t	codepointCount;
	uint32_t	ttfDataSize;
};

typedef struct ImAppResPakThemeData ImAppResPakThemeData;
struct ImAppResPakThemeData
{
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
};

#ifdef __cplusplus
}
#endif

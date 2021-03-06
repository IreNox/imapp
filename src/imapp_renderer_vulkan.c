#include "imapp_renderer.h"

#if IMAPP_ENABLED( IMAPP_RENDERER_VULKAN )

#include <vulkan/vulkan.h>

struct ImAppRenderer
{
	ImAppAllocator*				pAllocator;

	VkInstance					instance;
	VkDevice					device;

	bool						nkCreated;
	struct nk_font_atlas		nkFontAtlas;
	struct nk_convert_config	nkConvertConfig;
	struct nk_buffer			nkCommands;

	ImAppRendererTexture*		pFirstTexture;
};

struct ImAppRendererTexture
{
	ImAppRendererTexture*		pNext;

	int							width;
	int							height;

	void*						pData;
	size_t						size;
};


#endif

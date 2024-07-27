#pragma once

#include <tiki/tiki_dynamic_string.h>

namespace imapp
{
	using namespace tiki;

	enum class ResourceType
	{
		Image,
		Skin,
		Font,
		Theme,

		Count
	};

	struct ResourceFontUnicodeBlock
	{
		DynamicString			name;
		uint32					first	= 0u;
		uint32					last	= 0u;
	};
}
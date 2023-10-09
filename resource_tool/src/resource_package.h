#pragma once

#include "resource_helpers.h"

#include <imapp/../../src/imapp_res_pak.h>

#include <tiki/tiki_dynamic_array.h>
#include <tiki/tiki_dynamic_string.h>

#include <tinyxml2.h>

namespace imapp
{
	using namespace tiki;
	using namespace tinyxml2;

	class Resource;
	enum class ResourceType;

	class ResourcePackage
	{
	public:

						ResourcePackage();
						~ResourcePackage();

		bool			load( const StringView& filename );

		bool			hasPath() const;
		bool			save();
		bool			saveAs( const StringView& filename );

		void			updateFileData( float time );

		void			addResource( const StringView& name, ResourceType type );
		Resource&		getResource( uintsize index );
		uintsize		getResourceCount() const;

	private:

		using ResourceArray = DynamicArray< Resource >;

		DynamicString	m_path;
		XMLDocument		m_xml;

		DynamicString	m_name;
		ResourceArray	m_resources;
	};
}

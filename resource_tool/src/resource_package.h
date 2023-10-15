#pragma once

#include "resource_helpers.h"

#include <tiki/tiki_dynamic_array.h>
#include <tiki/tiki_dynamic_string.h>

#include <tinyxml2.h>

struct ImAppContext;

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

		bool					load( const StringView& filename );

		bool					hasPath() const;
		bool					save();
		bool					saveAs( const StringView& filename );

		void					updateFileData( ImAppContext* imapp, float time );

		StringView				getPath() const { return m_path; }
		StringView				getName() const { return m_name; }

		Resource&				addResource( const StringView& name, ResourceType type );
		Resource&				getResource( uintsize index );
		void					removeResource( uintsize index );
		uintsize				getResourceCount() const;
		ArrayView< Resource >	getResources() const { return m_resources; }

		Resource*				findResource( const StringView& name );

		uint32					getRevision() const;

	private:

		using ResourceArray = DynamicArray< Resource >;

		uint32					m_revision;

		DynamicString			m_path;
		XMLDocument				m_xml;

		DynamicString			m_name;
		ResourceArray			m_resources;
	};
}

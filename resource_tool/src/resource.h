#pragma once

#include "resource_helpers.h"

#include <imapp/imapp.h>
#include <imui/imui_cpp.h>

#include <tiki/tiki_dynamic_string.h>

#include <tinyxml2.h>

struct ImAppImage;

namespace imapp
{
	using namespace imui::toolbox;
	using namespace imui;
	using namespace tiki;
	using namespace tinyxml2;

	enum class ResourceType
	{
		Image,
		Skin,
		Config
	};

	ArrayView< StringView >	getResourceTypeStrings();
	bool					parseResourceType( ResourceType& type, const StringView& string );
	StringView				getResourceTypeString( ResourceType type );

	class Resource
	{
	public:

						Resource();
						Resource( const StringView& name, ResourceType type, const StringView& sourcePath );
						~Resource();

		bool			load( XMLElement* resourceNode );
		bool			serialize( XMLElement* resourcesNode );

		StringView		getName() const { return m_name; }
		void			setName( const StringView& value );

		StringView		getSourcePath() const { return m_path; }
		void			setSourcePath( const StringView& value );

		ResourceType	getType() const { return m_type; }

	private:

		DynamicString	m_name;
		DynamicString	m_path;
		ResourceType	m_type;

		XMLElement*		m_xml		= nullptr;

		ImAppImage*		m_image		= nullptr;
		UiBorder		m_skinBorder;
		UiToolboxConfig	m_config;
	};
}

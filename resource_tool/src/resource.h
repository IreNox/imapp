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
						Resource( const StringView& name, ResourceType type );
						~Resource();

		bool			load( XMLElement* resourceNode );
		void			serialize( XMLElement* resourcesNode );

		void			updateFileData( float time );

		StringView		getName() const { return m_name; }
		void			setName( const StringView& value );

		StringView		getImageSourcePath() const { return m_imageSourcePath; }
		void			setImageSourcePath( const StringView& value );

		ResourceType	getType() const { return m_type; }

	private:

		using ByteArray = DynamicArray< byte >;

		DynamicString	m_name;
		ResourceType	m_type			= ResourceType::Image;

		XMLElement*		m_xml			= nullptr;

		ByteArray		m_fileData;
		ImUiHash		m_fileHash		= 0u;
		float			m_fileCheckTime	= 0.0f;

		DynamicString	m_imageSourcePath;
		ByteArray		m_imageData;
		ImAppImage*		m_image			= nullptr;

		DynamicString	m_skinImageName;
		UiBorder		m_skinBorder;

		UiToolboxConfig	m_config;

		bool			loadImageXml();
		bool			loadSkinXml();
		bool			loadConfigXml();
		void			serializeImageXml();
		void			serializeSkinXml();
		void			serializeConfigXml();
	};
}

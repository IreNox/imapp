#pragma once

#include <imui/imui_cpp.h>

#include <tiki/tiki_static_array.h>
#include <tiki/tiki_dynamic_string.h>

namespace tiki
{
	class StringView;
}

namespace tinyxml2
{
	class XMLElement;
}

namespace imapp
{
	using namespace imui::toolbox;
	using namespace imui;
	using namespace tiki;
	using namespace tinyxml2;

	class Resource;
	class ResourcePackage;

	enum class ResourceToolboxConfigFieldType
	{
		Group,
		Font,
		Color,
		Skin,
		Float,
		Border,
		Size,
		Image,
		UInt32
	};

	struct ResourceToolboxConfigField
	{
		StringView						name;
		ResourceToolboxConfigFieldType	type	= ResourceToolboxConfigFieldType::Group;
		union
		{
			void*						dataPtr;
			DynamicString*				fontNamePtr;
			ImUiColor*					colorPtr;
			DynamicString*				skinNamePtr;
			float*						floatPtr;
			ImUiBorder*					borderPtr;
			ImUiSize*					sizePtr;
			DynamicString*				imageNamePtr;
			uint32*						uintPtr;
		}								data;
		XMLElement*						xml		= nullptr;
	};

	class ResourceToolboxConfig
	{
	public:

		using FieldView = Array< ResourceToolboxConfigField >;

								ResourceToolboxConfig();
								ResourceToolboxConfig( const ResourceToolboxConfig& config );

		bool					load( XMLElement* resourceNode );
		void					serialize( XMLElement* resourceNode );

		FieldView				getFields() { return m_fields; }

		StringView				getFontName() const { return m_fontName; }
		void					setFontName( const StringView& value ) { m_fontName = value; }
		Resource*				findFont( ResourcePackage& package );

		ResourceToolboxConfig&	operator=( const ResourceToolboxConfig& rhs );

	private:

		static constexpr uintsize FieldCount = ImUiToolboxColor_MAX + ImUiToolboxSkin_MAX + ImUiToolboxImage_MAX + 63u;

		using FieldArray = StaticArray< ResourceToolboxConfigField, FieldCount >;
		using SkinArray = StaticArray< DynamicString, ImUiToolboxSkin_MAX >;
		using ImageArray = StaticArray< DynamicString, ImUiToolboxImage_MAX >;

		UiToolboxConfig			m_config;

		FieldArray				m_fields;

		DynamicString			m_fontName;
		SkinArray				m_skins;
		ImageArray				m_images;

		void					setFields();
	};
}
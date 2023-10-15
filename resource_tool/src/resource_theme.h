#pragma once

#include <imui/imui_toolbox.h>

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

	enum class ResourceThemeFieldType
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

	struct ResourceThemeField
	{
		StringView				name;
		ResourceThemeFieldType	type	= ResourceThemeFieldType::Group;
		union
		{
			void*				dataPtr;
			DynamicString*		fontNamePtr;
			ImUiColor*			colorPtr;
			DynamicString*		skinNamePtr;
			float*				floatPtr;
			ImUiBorder*			borderPtr;
			ImUiSize*			sizePtr;
			DynamicString*		imageNamePtr;
			uint32*				uintPtr;
		}						data;
		XMLElement*				xml		= nullptr;
	};

	class ResourceTheme
	{
	public:

		using FieldView = Array< ResourceThemeField >;

						ResourceTheme();
						ResourceTheme( const ResourceTheme& theme );

		bool			load( XMLElement* resourceNode );
		void			serialize( XMLElement* resourceNode );

		FieldView		getFields() { return m_fields; }

		StringView		getFontName() const { return m_fontName; }
		void			setFontName( const StringView& value ) { m_fontName = value; }

		ResourceTheme&	operator=( const ResourceTheme& rhs );

	private:

		static constexpr uintsize FieldCount = ImUiToolboxColor_MAX + ImUiToolboxSkin_MAX + ImUiToolboxImage_MAX + 41u;

		using FieldArray = StaticArray< ResourceThemeField, FieldCount >;
		using SkinArray = StaticArray< DynamicString, ImUiToolboxSkin_MAX >;
		using ImageArray = StaticArray< DynamicString, ImUiToolboxImage_MAX >;

		UiToolboxConfig	m_config;

		FieldArray		m_fields;

		DynamicString	m_fontName;
		SkinArray		m_skins;
		ImageArray		m_images;

		void			setFields();
	};
}
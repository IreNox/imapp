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
		UInt32,
		Time
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
			double*				doublePtr;
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

		using FieldView = ArrayView< ResourceThemeField >;

								ResourceTheme();
								ResourceTheme( const ResourceTheme& theme );

		bool					load( XMLElement* resourceNode );
		void					serialize( XMLElement* resourceNode );

		FieldView				getFields() { return m_fields; }

		StringView				getFontName() const { return m_fontName; }
		void					setFontName( const StringView& value ) { m_fontName = value; }

		const UiToolboxConfig&	getConfig() const { return m_config; }
		StringView				getSkinName( ImUiToolboxSkin skin ) const { return m_skins[ skin ]; }
		StringView				getIconName( ImUiToolboxIcon icon ) const { return m_icons[ icon ]; }

		ResourceTheme&			operator=( const ResourceTheme& rhs );

	private:

		static constexpr uintsize FieldCount = ImUiToolboxColor_MAX + ImUiToolboxSkin_MAX + ImUiToolboxIcon_MAX + 41u;

		using FieldArray = StaticArray< ResourceThemeField, FieldCount >;
		using SkinArray = StaticArray< DynamicString, ImUiToolboxSkin_MAX >;
		using ImageArray = StaticArray< DynamicString, ImUiToolboxIcon_MAX >;

		UiToolboxConfig			m_config;

		FieldArray				m_fields;

		DynamicString			m_fontName;
		SkinArray				m_skins;
		ImageArray				m_icons;

		void					setFields();
	};
}
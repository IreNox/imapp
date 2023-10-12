#pragma once

#include <imui/imui_cpp.h>

#include <tiki/tiki_static_array.h>
#include <tiki/tiki_dynamic_string.h>

namespace tiki
{
	class StringView;
}

namespace imapp
{
	using namespace imui::toolbox;
	using namespace imui;
	using namespace tiki;

	class Resource;
	class ResourcePackage;

	enum class ResourceToolboxConfigFieldType
	{
		Float,
		Border,
		Size,
		Texture,
		UInt32
	};

	struct ResourceToolboxConfigField
	{
		StringView						name;
		ResourceToolboxConfigFieldType	type;
		union
		{
			void*						dataPtr;
			float*						floatPtr;
			ImUiBorder*					borderPtr;
			ImUiSize*					sizePtr;
			ImUiTexture*				texturePtr;
			uint32*						uintPtr;
		}								data;
	};

	class ResourceToolboxConfig
	{
	public:

		using FieldView = ArrayView< ResourceToolboxConfigField >;

							ResourceToolboxConfig();

		FieldView			getFields() { return m_fields; }

		StringView			getFontName() const { return m_fontName; }
		void				setFontName( const StringView& value );
		Resource*			findFont( ResourcePackage& package );

	private:

		using FieldArray = StaticArray< ResourceToolboxConfigField, 32u >;
		using ColorArray = StaticArray< UiColor, ImUiToolboxColor_MAX >;
		using SkinArray = StaticArray< DynamicString, ImUiToolboxSkin_MAX >;

		UiToolboxConfig		m_config;

		FieldArray			m_fields;

		DynamicString		m_fontName;
		ColorArray			m_colors;
		SkinArray			m_skins;
	};
}
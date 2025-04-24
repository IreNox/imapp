#pragma once

#include <imapp/../../src/imapp_res_pak.h>

#include <tiki/tiki_chunked_pool.h>
#include <tiki/tiki_dynamic_string.h>
#include <tiki/tiki_linked_list.h>
#include <tiki/tiki_static_array.h>

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

	struct ResourceThemeTreeField;

	struct ResourceThemeField
	{
		using UiField = ImUiToolboxThemeReflectionField;

		DynamicString				name;
		const UiField*				uiField	= nullptr;
		ImAppResPakThemeFieldBase	base;
		uintsize					level;

		XMLElement*					xml		= nullptr;
		DynamicString				value;
	};

	class ResourceTheme
	{
	public:

		using FieldView = ArrayView< ResourceThemeField >;
		using ConstFieldView = ConstArrayView< ResourceThemeField >;

								ResourceTheme();
								ResourceTheme( const ResourceTheme& theme );

		bool					load( XMLElement* resourceNode );
		void					serialize( XMLElement* resourceNode );

		FieldView				getFields() { return m_fields; }
		ConstFieldView			getFields() const { return m_fields; }

		ImUiColor&				getFieldColor( ResourceThemeField& field );
		const ImUiColor&		getFieldColor( const ResourceThemeField& field ) const;
		float&					getFieldFloat( ResourceThemeField& field );
		const float&			getFieldFloat( const ResourceThemeField& field ) const;
		double&					getFieldDouble( ResourceThemeField& field );
		const double&			getFieldDouble( const ResourceThemeField& field ) const;
		ImUiBorder&				getFieldBorder( ResourceThemeField& field );
		const ImUiBorder&		getFieldBorder( const ResourceThemeField& field ) const;
		ImUiSize&				getFieldSize( ResourceThemeField& field );
		const ImUiSize&			getFieldSize( const ResourceThemeField& field ) const;
		uint32&					getFieldUInt32( ResourceThemeField& field );
		const uint32&			getFieldUInt32( const ResourceThemeField& field ) const;
		DynamicString&			getFieldString( ResourceThemeField& field );
		const DynamicString&	getFieldString( const ResourceThemeField& field ) const;

		const ImUiToolboxTheme&	getUiTheme() const { return m_theme.uiTheme; }
		const ImAppWindowTheme&	getWindowTheme() const { return m_theme.windowTheme; }

	private:

		static constexpr uintsize FieldCount = ImUiToolboxColor_MAX + ImUiToolboxSkin_MAX + ImUiToolboxIcon_MAX + 38u;

		using FieldArray = DynamicArray< ResourceThemeField >;
		using SkinArray = StaticArray< DynamicString, ImUiToolboxSkin_MAX >;
		using ImageArray = StaticArray< DynamicString, ImUiToolboxIcon_MAX >;

		using FieldPool = ChunkedPool< ResourceThemeTreeField >;

		ImAppTheme				m_theme;

		FieldArray				m_fields;

		void					buildFields();
		void					buildReflectionFields( const ImUiToolboxThemeReflection& reflection, ResourceThemeTreeField& rootField, FieldPool& fieldPool, ImAppResPakThemeFieldBase base );
		void					serializeFields( DynamicArray< ResourceThemeTreeField* >& fields, ResourceThemeTreeField& field, uintsize level );

		void*					getFieldData( ResourceThemeField& field );
	};
}
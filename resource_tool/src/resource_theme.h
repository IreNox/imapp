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
		using UiField = ImuiToolboxThemeReflectionField;

		DynamicString				name;
		const UiField*				uiField	= nullptr;
		ImappResPakThemeFieldBase	base;
		uintsize					level;
		bool						open	= false;

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

		ImuiColor&				getFieldColor( ResourceThemeField& field );
		const ImuiColor&		getFieldColor( const ResourceThemeField& field ) const;
		float&					getFieldFloat( ResourceThemeField& field );
		const float&			getFieldFloat( const ResourceThemeField& field ) const;
		double&					getFieldDouble( ResourceThemeField& field );
		const double&			getFieldDouble( const ResourceThemeField& field ) const;
		ImuiBorder&				getFieldBorder( ResourceThemeField& field );
		const ImuiBorder&		getFieldBorder( const ResourceThemeField& field ) const;
		ImuiSize&				getFieldSize( ResourceThemeField& field );
		const ImuiSize&			getFieldSize( const ResourceThemeField& field ) const;
		uint32&					getFieldUInt32( ResourceThemeField& field );
		const uint32&			getFieldUInt32( const ResourceThemeField& field ) const;
		DynamicString&			getFieldString( ResourceThemeField& field );
		const DynamicString&	getFieldString( const ResourceThemeField& field ) const;

		const ImuiToolboxTheme&	getUiTheme() const { return m_theme.uiTheme; }
		const ImappWindowTheme&	getWindowTheme() const { return m_theme.windowTheme; }

	private:

		static constexpr uintsize FieldCount = ImuiToolboxColor_MAX + ImuiToolboxSkin_MAX + ImuiToolboxIcon_MAX + 38u;

		using FieldArray = DynamicArray< ResourceThemeField >;
		using SkinArray = StaticArray< DynamicString, ImuiToolboxSkin_MAX >;
		using ImageArray = StaticArray< DynamicString, ImuiToolboxIcon_MAX >;

		using FieldPool = ChunkedPool< ResourceThemeTreeField >;

		ImappTheme				m_theme;

		FieldArray				m_fields;

		void					buildFields();
		void					buildReflectionFields( const ImuiToolboxThemeReflection& reflection, ResourceThemeTreeField& rootField, FieldPool& fieldPool, ImappResPakThemeFieldBase base );
		void					serializeFields( DynamicArray< ResourceThemeTreeField* >& fields, ResourceThemeTreeField& field, uintsize level );

		void*					getFieldData( ResourceThemeField& field );
	};
}
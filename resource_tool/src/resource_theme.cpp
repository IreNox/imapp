#include "resource_theme.h"

#include <tiki/tiki_string_tools.h>

#include <tinyxml2.h>

namespace imapp
{
	using namespace tiki;

	struct ResourceThemeTreeField : public LinkedNode< ResourceThemeTreeField >
	{
		using FieldList = LinkedList< ResourceThemeTreeField >;
		using UiField = ImuiToolboxThemeReflectionField;

		StringView					name;
		const UiField*				uiField	= nullptr;
		ImappResPakThemeFieldBase	base;

		FieldList					childFields;
	};

	ResourceTheme::ResourceTheme()
	{
		imuiToolboxThemeFillDefault( &m_theme.uiTheme, nullptr );
		imappWindowThemeFillDefault( &m_theme.windowTheme );

		buildFields();
	}

	ResourceTheme::ResourceTheme( const ResourceTheme& theme )
	{
		*this = theme;
	}

	bool ResourceTheme::load( XMLElement* resourceNode )
	{
		for( XMLElement* configNode = resourceNode->FirstChildElement( "value" ); configNode != nullptr; configNode = configNode->NextSiblingElement( "value" ) )
		{
			const char* name;
			if( configNode->QueryStringAttribute( "name", &name ) != XML_SUCCESS )
			{
				continue;
			}

			const StringView nameView = StringView( name );
			for( ResourceThemeField& field : m_fields )
			{
				if( !field.uiField ||
					nameView != field.uiField->name )
				{
					continue;
				}

				if( field.xml )
				{
					resourceNode->DeleteChild( field.xml );
				}

				field.xml = configNode;

				bool ok = true;
				const ImuiToolboxThemeReflectionField& uiField = *field.uiField;
				switch( uiField.type )
				{
				case ImuiToolboxThemeReflectionType_Font:
					{
						const char* fontName;
						if( configNode->QueryStringAttribute( "fontName", &fontName ) == XML_SUCCESS )
						{
							field.value = fontName;
						}
						else
						{
							ok = false;
						}
					}
					break;

				case ImuiToolboxThemeReflectionType_Color:
					{
						const char* colorHex;
						if( configNode->QueryStringAttribute( "color", &colorHex ) == XML_SUCCESS )
						{
							parseUiColor( getFieldColor( field ), StringView( colorHex ) );
						}
						else
						{
							ok = false;
						}
					}
					break;

				case ImuiToolboxThemeReflectionType_Skin:
					{
						const char* skinName;
						if( configNode->QueryStringAttribute( "skinName", &skinName ) == XML_SUCCESS )
						{
							field.value = skinName;
						}
						else
						{
							ok = false;
						}
					}
					break;

				case ImuiToolboxThemeReflectionType_Float:
					{
						float& target = getFieldFloat( field );
						ok &= configNode->QueryFloatAttribute( "value", &target ) == XML_SUCCESS;
					}
					break;

				case ImuiToolboxThemeReflectionType_Double:
					{
						double& target = getFieldDouble( field );
						ok &= configNode->QueryDoubleAttribute( "value", &target ) == XML_SUCCESS;
					}
					break;

				case ImuiToolboxThemeReflectionType_Border:
					{
						ImuiBorder& target = getFieldBorder( field );
						ok &= configNode->QueryFloatAttribute( "top", &target.top ) == XML_SUCCESS;
						ok &= configNode->QueryFloatAttribute( "left", &target.left ) == XML_SUCCESS;
						ok &= configNode->QueryFloatAttribute( "right", &target.right ) == XML_SUCCESS;
						ok &= configNode->QueryFloatAttribute( "bottom", &target.bottom ) == XML_SUCCESS;
					}
					break;

				case ImuiToolboxThemeReflectionType_Size:
					{
						ImuiSize& target = getFieldSize( field );
						ok &= configNode->QueryFloatAttribute( "width", &target.width ) == XML_SUCCESS;
						ok &= configNode->QueryFloatAttribute( "height", &target.height ) == XML_SUCCESS;
					}
					break;

				case ImuiToolboxThemeReflectionType_Image:
					{
						const char* imageName;
						if( configNode->QueryStringAttribute( "imageName", &imageName ) == XML_SUCCESS )
						{
							field.value = imageName;
						}
					}
					break;

				case ImuiToolboxThemeReflectionType_UInt32:
					{
						uint32& target = getFieldUInt32( field );
						ok &= configNode->QueryIntAttribute( "value", (int*)&target ) == XML_SUCCESS;
					}
					break;
				}

				break;
			}
		}

		return true;
	}

	void ResourceTheme::serialize( XMLElement* resourceNode )
	{
		for( ResourceThemeField& field : m_fields )
		{
			if( !field.uiField )
			{
				continue;
			}

			if( !field.xml )
			{
				field.xml = resourceNode->GetDocument()->NewElement( "value" );
				resourceNode->InsertEndChild( field.xml );
			}

			field.xml->SetAttribute( "name", field.uiField->name );

			switch( field.uiField->type )
			{
			case ImuiToolboxThemeReflectionType_Font:
				field.xml->SetAttribute( "fontName", field.value.toConstCharPointer() );
				break;

			case ImuiToolboxThemeReflectionType_Color:
				{
					ImuiColor& data = getFieldColor( field );

					char buffer[ 32u ];
					formatUiColor( buffer, sizeof( buffer ), data );

					field.xml->SetAttribute( "color", buffer );
				}
				break;

			case ImuiToolboxThemeReflectionType_Skin:
				field.xml->SetAttribute( "skinName", field.value.toConstCharPointer() );
				break;

			case ImuiToolboxThemeReflectionType_Float:
				{
					float& data = getFieldFloat( field );
					field.xml->SetAttribute( "value", data );
				}
				break;

			case ImuiToolboxThemeReflectionType_Double:
				{
					double& data = getFieldDouble( field );
					field.xml->SetAttribute( "value", data );
				}
				break;

			case ImuiToolboxThemeReflectionType_Border:
				{
					ImuiBorder& data = getFieldBorder( field );
					field.xml->SetAttribute( "top", data.top );
					field.xml->SetAttribute( "left", data.left );
					field.xml->SetAttribute( "right", data.right );
					field.xml->SetAttribute( "bottom", data.bottom );
				}
				break;

			case ImuiToolboxThemeReflectionType_Size:
				{
					ImuiSize& data = getFieldSize( field );
					field.xml->SetAttribute( "width", data.width );
					field.xml->SetAttribute( "height", data.height );
				}
				break;

			case ImuiToolboxThemeReflectionType_Image:
				field.xml->SetAttribute( "imageName", field.value.toConstCharPointer() );
				break;

			case ImuiToolboxThemeReflectionType_UInt32:
				{
					uint32& data = getFieldUInt32( field );
					field.xml->SetAttribute( "value", data );
				}
				break;
			}
		}
	}

	ImuiColor& ResourceTheme::getFieldColor( ResourceThemeField& field )
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Color );
		return *(ImuiColor*)getFieldData( field );
	}

	const ImuiColor& ResourceTheme::getFieldColor( const ResourceThemeField& field ) const
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Color );
		return const_cast<ResourceTheme* >( this )->getFieldColor( const_cast< ResourceThemeField& >( field ) );
	}

	float& ResourceTheme::getFieldFloat( ResourceThemeField& field )
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Float );
		return *(float*)getFieldData( field );
	}

	const float& ResourceTheme::getFieldFloat( const ResourceThemeField& field ) const
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Float );
		return const_cast<ResourceTheme* >( this )->getFieldFloat( const_cast< ResourceThemeField& >( field ) );
	}

	double& ResourceTheme::getFieldDouble( ResourceThemeField& field )
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Double );
		return *(double*)getFieldData( field );
	}

	const double& ResourceTheme::getFieldDouble( const ResourceThemeField& field ) const
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Double );
		return const_cast<ResourceTheme* >( this )->getFieldDouble( const_cast< ResourceThemeField& >( field ) );
	}

	ImuiBorder& ResourceTheme::getFieldBorder( ResourceThemeField& field )
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Border );
		return *(ImuiBorder*)getFieldData( field );
	}

	const ImuiBorder& ResourceTheme::getFieldBorder( const ResourceThemeField& field ) const
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Border );
		return const_cast<ResourceTheme* >( this )->getFieldBorder( const_cast< ResourceThemeField& >( field ) );
	}

	ImuiSize& ResourceTheme::getFieldSize( ResourceThemeField& field )
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Size );
		return *(ImuiSize*)getFieldData( field );
	}

	const ImuiSize& ResourceTheme::getFieldSize( const ResourceThemeField& field ) const
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_Size );
		return const_cast<ResourceTheme* >( this )->getFieldSize( const_cast< ResourceThemeField& >( field ) );
	}

	uint32& ResourceTheme::getFieldUInt32( ResourceThemeField& field )
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_UInt32 );
		return *(uint32*)getFieldData( field );
	}

	const uint32& ResourceTheme::getFieldUInt32( const ResourceThemeField& field ) const
	{
		TIKI_ASSERT( field.uiField && field.uiField->type == ImuiToolboxThemeReflectionType_UInt32 );
		return const_cast<ResourceTheme* >( this )->getFieldUInt32( const_cast< ResourceThemeField& >( field ) );
	}

	DynamicString& ResourceTheme::getFieldString( ResourceThemeField& field )
	{
		return field.value;
	}

	const tiki::DynamicString& ResourceTheme::getFieldString( const ResourceThemeField& field ) const
	{
		return field.value;
	}

	void ResourceTheme::buildFields()
	{
		FieldPool pool( 256u );
		ResourceThemeTreeField rootField;

		const ImuiToolboxThemeReflection uiReflection = imuiToolboxThemeReflectionGet();
		buildReflectionFields( uiReflection, rootField, pool, ImappResPakThemeFieldBase_UiTheme );

		{
			ResourceThemeTreeField* windowRootField = pool.push();
			windowRootField->name	= "Window";
			windowRootField->base	= (ImappResPakThemeFieldBase)-1;

			rootField.childFields.push( windowRootField );

			const ImuiToolboxThemeReflection appReflection = imappWindowThemeReflectionGet();
			buildReflectionFields( appReflection, *windowRootField, pool, ImappResPakThemeFieldBase_WindowTheme );
		}

		DynamicArray< ResourceThemeTreeField* > fields;
		serializeFields( fields, rootField, 0u );

		for( ResourceThemeTreeField* field : fields )
		{
			pool.eraseUnsortedByValue( field );
		}
	}

	void ResourceTheme::buildReflectionFields( const ImuiToolboxThemeReflection& reflection, ResourceThemeTreeField& rootField, FieldPool& fieldPool, ImappResPakThemeFieldBase base )
	{
		for( uintsize i = 0u; i < reflection.count; ++i )
		{
			const ImuiToolboxThemeReflectionField& field = reflection.fields[ i ];

			ResourceThemeTreeField* parentField = &rootField;

			StringView name = StringView( field.name );
			while( name.contains( "/" ) )
			{
				const StringView namePart = name.subString( 0u, name.indexOf( "/" ) );

				ResourceThemeTreeField* partField = nullptr;
				for( ResourceThemeTreeField& childField : parentField->childFields )
				{
					if( childField.name != namePart )
					{
						continue;
					}

					partField = &childField;
					break;
				}

				if( !partField )
				{
					partField = fieldPool.push();
					partField->name	= namePart;
					partField->base	= (ImappResPakThemeFieldBase)-1;

					parentField->childFields.push( partField );
				}

				parentField = partField;
				name = StringView( namePart.getEnd() + 1u, name.getEnd() );
			}

			ResourceThemeTreeField* targetField = fieldPool.push();
			targetField->name		= name;
			targetField->uiField	= &field;
			targetField->base		= base;

			parentField->childFields.push( targetField );
		}
	}

	void ResourceTheme::serializeFields( DynamicArray< ResourceThemeTreeField* >& fields, ResourceThemeTreeField& field, uintsize level )
	{
		// value fields first
		for( ResourceThemeTreeField& childField : field.childFields )
		{
			if( !childField.uiField )
			{
				continue;
			}
			TIKI_ASSERT( childField.childFields.isEmpty() );

			ResourceThemeField& serialChildField = m_fields.pushBack();
			serialChildField.name		= childField.name;
			serialChildField.uiField	= childField.uiField;
			serialChildField.base		= childField.base;
			serialChildField.level		= level;

			serializeFields( fields, childField, level + 1u );
			fields.pushBack( &childField );
		}

		for( ResourceThemeTreeField& childField : field.childFields )
		{
			if( childField.uiField )
			{
				continue;
			}

			ResourceThemeField& serialChildField = m_fields.pushBack();
			serialChildField.name		= childField.name;
			serialChildField.uiField	= childField.uiField;
			serialChildField.base		= childField.base;
			serialChildField.level		= level;

			serializeFields( fields, childField, level + 1u );
			fields.pushBack( &childField );
		}


		field.childFields.clear();
	}

	void* ResourceTheme::getFieldData( ResourceThemeField& field )
	{
		byte* base = nullptr;
		if( field.base == ImappResPakThemeFieldBase_UiTheme )
		{
			base = (byte*)&m_theme.uiTheme;
		}
		else if( field.base == ImappResPakThemeFieldBase_WindowTheme )
		{
			base = (byte*)&m_theme.windowTheme;
		}
		else
		{
			return nullptr;
		}

		return base + field.uiField->offset;
	}
}
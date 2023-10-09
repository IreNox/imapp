#include "resource.h"

#include <stdio.h>

namespace imapp
{
	using namespace imui;
	using namespace tiki;

	static constexpr float FileCheckInterval = 5.0f;

	static const StringView s_resourceTypeStrings[] =
	{
		"Image",
		"Skin",
		"Config"
	};

	ArrayView< StringView > getResourceTypeStrings()
	{
		return ArrayView< StringView >( s_resourceTypeStrings, TIKI_ARRAY_COUNT( s_resourceTypeStrings ) );
	}

	bool parseResourceType( ResourceType& type, const StringView& string )
	{
		for( size_t i = 0u; i < TIKI_ARRAY_COUNT( s_resourceTypeStrings ); ++i )
		{
			const StringView& mapping = s_resourceTypeStrings[ i ];
			if( string == mapping )
			{
				type = (ResourceType)i;
				return true;
			}
		}

		return false;
	}

	StringView getResourceTypeString( ResourceType type )
	{
		return s_resourceTypeStrings[ (size_t)type ];
	}

	Resource::Resource()
	{
	}

	Resource::Resource( const StringView& name, ResourceType type )
		: m_name( name )
		, m_type( type )
	{

	}

	Resource::~Resource()
	{

	}

	bool Resource::load( XMLElement* resourceNode )
	{
		m_xml = resourceNode;

		const char* name;
		if( m_xml->QueryStringAttribute( "name", &name ) != XML_SUCCESS )
		{
			return false;
		}
		m_name = name;

		const char* type;
		if( m_xml->QueryStringAttribute( "type", &type ) != XML_SUCCESS )
		{
			return false;
		}

		if( !parseResourceType( m_type, StringView( type ) ) )
		{
			return false;
		}

		switch( m_type )
		{
		case imapp::ResourceType::Image:
			return loadImageXml();

		case imapp::ResourceType::Skin:
			return loadSkinXml();

		case imapp::ResourceType::Config:
			return loadConfigXml();
		}

		return false;
	}

	void Resource::serialize( XMLElement* resourcesNode )
	{
		if( !m_xml )
		{
			m_xml = resourcesNode->GetDocument()->NewElement( "resource" );
			resourcesNode->InsertEndChild( m_xml );
		}

		m_xml->SetAttribute( "name", m_name );
		m_xml->SetAttribute( "type", getResourceTypeString( m_type ) );

		switch( m_type )
		{
		case ResourceType::Image:
			serializeImageXml();
			break;

		case ResourceType::Skin:
			serializeSkinXml();
			break;

		case ResourceType::Config:
			serializeConfigXml();
			break;
		}
	}

	void Resource::updateFileData( float time )
	{
		if( m_type != ResourceType::Image ||
			time - m_fileCheckTime < FileCheckInterval )
		{
			return;
		}

		// TODO: check file modified time

		m_fileCheckTime = time;

		FILE* file = fopen( m_imageSourcePath.getData(), "rb" );
		if( !file )
		{
			m_fileData.clear();
			m_fileHash = 0u;
			return;
		}

		fseek( file, 0, SEEK_END );
		const size_t fileSize = ftell( file );
		fseek( file, 0, SEEK_SET );

		m_fileData.setLengthUninitialized( fileSize );
		const size_t readResult = fread( m_fileData.getData(), fileSize, 1u, file );
		fclose( file );
		if( readResult != 1u )
		{
			return;
		}

		const ImUiHash newFileHash = ImUiHashCreate( m_fileData.getData(), m_fileData.getLength(), 0u );
		if( newFileHash == m_fileHash )
		{
			return;
		}

		//if( m_image )
//{
//	ImAppImageFree( ..., m_image )
//	m_image = nullptr;
//}

//m_image = ImAppImageLoadFromMemory( ..., m_fileData.getData(), m_fileData.getLength(), )

		m_fileHash = newFileHash;
	}

	void Resource::setName( const StringView& value )
	{
		m_name = value;
	}

	void Resource::setImageSourcePath( const StringView& value )
	{
		m_imageSourcePath = value;
	}

	bool Resource::loadImageXml()
	{
		const char* path;
		if( m_xml->QueryStringAttribute( "path", &path ) != XML_SUCCESS )
		{
			return false;
		}
		m_imageSourcePath = path;

		return true;
	}

	bool Resource::loadSkinXml()
	{
		const char* image_name;
		if( m_xml->QueryStringAttribute( "image_name", &image_name ) != XML_SUCCESS )
		{
			return false;
		}
		m_skinImageName = image_name;

		return m_xml->QueryFloatAttribute( "top", &m_skinBorder.top ) == XML_SUCCESS &&
			m_xml->QueryFloatAttribute( "left", &m_skinBorder.left ) == XML_SUCCESS &&
			m_xml->QueryFloatAttribute( "bottom", &m_skinBorder.bottom ) == XML_SUCCESS &&
			m_xml->QueryFloatAttribute( "right", &m_skinBorder.right ) == XML_SUCCESS;
	}

	bool Resource::loadConfigXml()
	{
		return true;
	}

	void Resource::serializeImageXml()
	{
		m_xml->SetAttribute( "path", m_imageSourcePath );
	}

	void Resource::serializeSkinXml()
	{
		m_xml->SetAttribute( "image_name", m_skinImageName );

		m_xml->SetAttribute( "top", m_skinBorder.top );
		m_xml->SetAttribute( "left", m_skinBorder.left );
		m_xml->SetAttribute( "bottom", m_skinBorder.bottom );
		m_xml->SetAttribute( "right", m_skinBorder.right );
	}

	void Resource::serializeConfigXml()
	{

	}
}

#include "resource.h"

namespace imapp
{
	using namespace imui;
	using namespace tiki;

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

	Resource::Resource( const StringView& name, ResourceType type, const StringView& sourcePath )
		: m_name( name )
		, m_type( type )
		, m_path( sourcePath )
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

		const char* path;
		if( m_xml->QueryStringAttribute( "path", &path ) != XML_SUCCESS )
		{
			return false;
		}
		m_path = path;

		return true;
	}

	bool Resource::serialize( XMLElement* resourcesNode )
	{
		if( !m_xml )
		{
			m_xml = resourcesNode->GetDocument()->NewElement( "resource" );
			resourcesNode->InsertEndChild( m_xml );
		}

		m_xml->SetAttribute( "name", m_name );
		m_xml->SetAttribute( "type", getResourceTypeString( m_type ) );
		m_xml->SetAttribute( "path", m_path );

		return true;
	}

	void Resource::setName( const StringView& value )
	{
		m_name = value;
	}

	void Resource::setSourcePath( const StringView& value )
	{
		m_path = value;
	}
}

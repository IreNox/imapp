#include "resource_package.h"

#include "resource.h"

namespace imapp
{
	using namespace tinyxml2;

	ResourcePackage::ResourcePackage()
	{
	}

	ResourcePackage::~ResourcePackage()
	{
	}

	bool ResourcePackage::load( const RtStr& filename )
	{
		m_path = filename;

		if( m_xml.LoadFile( filename.getData() ) != XML_SUCCESS )
		{
			return false;
		}

		XMLElement* rootNode = m_xml.FirstChildElement( "res_pak" );
		if( !rootNode )
		{
			return false;
		}

		XMLElement* resourcesNode = rootNode->FirstChildElement( "resources" );
		if( resourcesNode )
		{
			for( XMLElement* resourceNode = resourcesNode->FirstChildElement( "resource" ); resourceNode; resourceNode = resourceNode->NextSiblingElement( "resource" ) )
			{
				Resource& resource = m_resources.pushBack();

				if( !resource.load( resourceNode ) )
				{
					return false;
				}
			}
		}

		return true;
	}

	bool ResourcePackage::hasPath() const
	{
		return m_path.hasElements();
	}

	bool ResourcePackage::save()
	{
		TIKI_ASSERT( hasPath() );
		return saveAs( m_path );
	}

	bool ResourcePackage::saveAs( const RtStr& filename )
	{
		XMLElement* rootNode = findOrCreateElement( &m_xml, "res_pak" );

		if( m_name.hasElements() )
		{
			rootNode->SetAttribute( "name", m_name );
		}

		XMLElement* resourcesNode = findOrCreateElement( rootNode, "resources" );

		for( Resource& resource : m_resources )
		{
			if( !resource.serialize( resourcesNode ) )
			{
				return false;
			}
		}

		return m_xml.SaveFile( filename.getData() );
	}

	void ResourcePackage::addResource( const StringView& name, ResourceType type, const StringView& sourceFilename )
	{
		Resource resource( name, type, sourceFilename );
		m_resources.pushBack( resource );
	}

	Resource& ResourcePackage::getResource( uintsize index )
	{
		return m_resources[ index ];
	}

	uintsize ResourcePackage::getResourceCount() const
	{
		return m_resources.getLength();
	}
}
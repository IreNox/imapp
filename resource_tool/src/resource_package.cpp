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
		for( uintsize i = 0u; i < m_resources.getLength(); ++i )
		{
			delete m_resources[ i ];
		}
	}

	bool ResourcePackage::load( const StringView& filename )
	{
		m_resources.clear();

		m_path = filename;

		if( m_xml.LoadFile( m_path.getNativePath().getData() ) != XML_SUCCESS )
		{
			return false;
		}

		XMLElement* rootNode = m_xml.FirstChildElement( "res_pak" );
		if( !rootNode )
		{
			return false;
		}

		{
			const char* name;
			if( rootNode->QueryStringAttribute( "name", &name ) != XML_SUCCESS )
			{
				return false;
			}
			m_name = name;
		}

		{
			const char* outputPath;
			if( rootNode->QueryStringAttribute( "outputPath", &outputPath ) != XML_SUCCESS )
			{
				return false;
			}
			m_outputPath = outputPath;
		}

		if( rootNode->QueryBoolAttribute( "exportCode", &m_exportCode ) != XML_SUCCESS )
		{
			m_exportCode = false;
		}

		XMLElement* resourcesNode = rootNode->FirstChildElement( "resources" );
		if( resourcesNode )
		{
			for( XMLElement* resourceNode = resourcesNode->FirstChildElement( "resource" ); resourceNode; resourceNode = resourceNode->NextSiblingElement( "resource" ) )
			{
				Resource* resource = new Resource();
				m_resources.pushBack( resource );

				if( !resource->load( resourceNode ) )
				{
					m_resources.popBack();
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
		return saveAs( m_path.getGenericPath() );
	}

	bool ResourcePackage::saveAs( const StringView& filename )
	{
		XMLElement* rootNode = findOrCreateElement( &m_xml, "res_pak" );

		m_path = filename;

		if( m_outputPath.isEmpty() )
		{
			const Path outputPath = m_path.replaceExtension( ".iarespak" );
			m_outputPath = outputPath.getFilename();
		}

		rootNode->SetAttribute( "name", m_name );
		rootNode->SetAttribute( "outputPath", m_outputPath );
		rootNode->SetAttribute( "exportCode", m_exportCode );

		XMLElement* resourcesNode = findOrCreateElement( rootNode, "resources" );

		for( Resource* resource : m_resources )
		{
			resource->serialize( resourcesNode );
		}

		return m_xml.SaveFile( m_path.getNativePath().getData() ) == XML_SUCCESS;
	}

	void ResourcePackage::updateFileData( double time )
	{
		for( Resource* resource : m_resources )
		{
			resource->updateFileData( m_path.getGenericPath(), time );
		}
	}

	Resource& ResourcePackage::addResource( const StringView& name, ResourceType type )
	{
		Resource* resource = new Resource( name, type );
		m_resources.pushBack( resource );
		return *resource;
	}

	Resource& ResourcePackage::getResource( uintsize index )
	{
		return *m_resources[ index ];
	}

	void ResourcePackage::removeResource( uintsize index )
	{
		Resource* resource = m_resources[ index ];
		resource->remove();
		delete resource;

		m_resources.eraseSortedByIndex( index );
	}

	uintsize ResourcePackage::getResourceCount() const
	{
		return m_resources.getLength();
	}

	Resource* ResourcePackage::findResource( const StringView& name )
	{
		for( Resource* resource : m_resources )
		{
			if( resource->getName() == name )
			{
				return resource;
			}
		}

		return nullptr;
	}

	uint32 ResourcePackage::getRevision() const
	{
		uint32 revision = m_revision;
		for( const Resource* resource : m_resources )
		{
			revision += resource->getRevision();
		}
		return revision;
	}
}
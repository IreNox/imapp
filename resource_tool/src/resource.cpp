#include "resource.h"

#include <tiki/tiki_path.h>

#include <spng/spng.h>
#include <tinyxml2.h>


namespace imapp
{
	using namespace imui;
	using namespace tiki;

	static constexpr double FileCheckInterval = 2.0;

	static const char* s_resourceTypeStrings[] =
	{
		"Image",
		"Skin",
		"Font",
		"Theme"
	};

	ArrayView< const char* > getResourceTypeStrings()
	{
		return ArrayView< const char* >( s_resourceTypeStrings, TIKI_ARRAY_COUNT( s_resourceTypeStrings ) );
	}

	bool parseResourceType( ResourceType& type, const StringView& string )
	{
		for( size_t i = 0u; i < TIKI_ARRAY_COUNT( s_resourceTypeStrings ); ++i )
		{
			const char* mapping = s_resourceTypeStrings[ i ];
			if( string == mapping )
			{
				type = (ResourceType)i;
				return true;
			}
		}

		return false;
	}

	const char* getResourceTypeString( ResourceType type )
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
		if( m_image )
		{
			ImAppImageFree( m_imapp, m_image );
			m_image = nullptr;
		}

		if( m_theme )
		{
			delete m_theme;
			m_theme = nullptr;
		}
	}

	bool Resource::load( XMLElement* resourceNode )
	{
		m_xml = resourceNode;

		m_revision++;

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
		case ResourceType::Image:
			return loadImageXml();

		case ResourceType::Skin:
			return loadSkinXml();

		case ResourceType::Font:
			return loadFontXml();

		case ResourceType::Theme:
			m_theme = new ResourceTheme();
			return m_theme->load( m_xml );
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

		case ResourceType::Font:
			serializeFontXml();
			break;

		case ResourceType::Theme:
			m_theme->serialize( m_xml );
			break;
		}
	}

	void Resource::remove()
	{
		if( m_xml )
		{
			m_xml->Parent()->DeleteChild( m_xml );
			m_xml = nullptr;
		}
	}

	void Resource::updateFileData( const Path& packagePath, double time )
	{
		if( (m_type != ResourceType::Image && m_type != ResourceType::Font) ||
			time - m_fileCheckTime < FileCheckInterval )
		{
			return;
		}

		// TODO: check file modified time

		m_fileCheckTime = time;

		Path filePath = packagePath.getParent().push( m_fileSourcePath );
		FILE* file = fopen( filePath.getNativePath().getData(), "rb" );

		if( !file &&
			m_type == ResourceType::Font )
		{
			filePath = Path( "c:/windows/fonts" ).push( m_fileSourcePath );
			file = fopen( filePath.getNativePath().getData(), "rb" );
		}

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

		const ImUiHash newFileHash = ImUiHashCreate( m_fileData.getData(), m_fileData.getLength() );
		if( newFileHash == m_fileHash )
		{
			return;
		}

		bool ok = true;
		switch( m_type )
		{
		case ResourceType::Image:
			ok = updateImageFileData();
			break;

		case ResourceType::Font:
			// ???
			break;

		case ResourceType::Skin:
		case ResourceType::Theme:
		case ResourceType::Count:
			break;
		}

		if( ok )
		{
			m_fileHash = newFileHash;
		}
		m_revision++;
	}

	void Resource::setImageAllowAtlas( bool value )
	{
		m_revision += (value != m_imageAllowAtlas);
		m_imageAllowAtlas = value;
	}

	void Resource::setImageRepeat( bool value )
	{
		m_revision += (value != m_imageRepeat);
		m_imageRepeat = value;
	}

	void Resource::setFontSize( float value )
	{
		m_revision += (value != m_fontSize);
		m_fontSize = value;
	}

	void Resource::setFontIsScalable( bool value )
	{
		m_revision += (value != m_fontIsScalable);
		m_fontIsScalable = value;
	}

	void Resource::setSkinImageName( const StringView& value )
	{
		m_skinImageName = value;
		m_revision++;
	}

	ImAppImage* Resource::getOrCreateImage( ImAppContext* imapp )
	{
		if( !m_image )
		{
			m_imapp			= imapp;
			m_image			= ImAppImageCreateRaw( imapp, m_imageData.getData(), m_imageData.getSizeInBytes(), (int)m_imageWidth, (int)m_imageHeight );
		}

		return m_image;
	}

	bool Resource::loadImageXml()
	{
		const char* path;
		if( m_xml->QueryStringAttribute( "path", &path ) != XML_SUCCESS )
		{
			return false;
		}
		m_fileSourcePath = path;

		m_xml->QueryBoolAttribute( "allow_atlas", &m_imageAllowAtlas );
		m_xml->QueryBoolAttribute( "repeat", &m_imageRepeat );

		return true;
	}

	bool Resource::loadSkinXml()
	{
		const char* imageName;
		if( m_xml->QueryStringAttribute( "image_name", &imageName ) != XML_SUCCESS )
		{
			return false;
		}
		m_skinImageName = imageName;

		return m_xml->QueryFloatAttribute( "top", &m_skinBorder.top ) == XML_SUCCESS &&
			m_xml->QueryFloatAttribute( "left", &m_skinBorder.left ) == XML_SUCCESS &&
			m_xml->QueryFloatAttribute( "bottom", &m_skinBorder.bottom ) == XML_SUCCESS &&
			m_xml->QueryFloatAttribute( "right", &m_skinBorder.right ) == XML_SUCCESS;
	}

	bool Resource::loadFontXml()
	{
		const char* path;
		if( m_xml->QueryStringAttribute( "path", &path ) != XML_SUCCESS ||
			m_xml->QueryFloatAttribute( "size", &m_fontSize ) != XML_SUCCESS ||
			m_xml->QueryBoolAttribute( "scalable", &m_fontIsScalable ) != XML_SUCCESS )
		{
			return false;
		}
		m_fileSourcePath = path;

		for( XMLElement* blockNode = m_xml->FirstChildElement( "block" ); blockNode; blockNode = blockNode->NextSiblingElement( "block" ) )
		{
			const char* name;
			uint32 first;
			uint32 last;
			if( blockNode->QueryStringAttribute( "name", &name ) != XML_SUCCESS ||
				blockNode->QueryUnsignedAttribute( "first", &first ) != XML_SUCCESS ||
				blockNode->QueryUnsignedAttribute( "last", &last ) != XML_SUCCESS )
			{
				return false;
			}

			ResourceFontUnicodeBlock& block = m_fontBlocks.pushBack();
			block.name	= name;
			block.first	= first;
			block.last	= last;
		}

		return true;
	}

	void Resource::serializeImageXml()
	{
		m_xml->SetAttribute( "path", m_fileSourcePath );
		m_xml->SetAttribute( "allow_atlas", m_imageAllowAtlas );
		m_xml->SetAttribute( "repeat", m_imageRepeat );
	}

	void Resource::serializeSkinXml()
	{
		m_xml->SetAttribute( "image_name", m_skinImageName );

		m_xml->SetAttribute( "top", m_skinBorder.top );
		m_xml->SetAttribute( "left", m_skinBorder.left );
		m_xml->SetAttribute( "bottom", m_skinBorder.bottom );
		m_xml->SetAttribute( "right", m_skinBorder.right );
	}

	void Resource::serializeFontXml()
	{
		m_xml->SetAttribute( "path", m_fileSourcePath );
		m_xml->SetAttribute( "size", m_fontSize );
		m_xml->SetAttribute( "scalable", m_fontIsScalable );

		m_xml->DeleteChildren();
		for( const ResourceFontUnicodeBlock& block : m_fontBlocks )
		{
			XMLElement* blockNode = m_xml->InsertNewChildElement( "block" );
			blockNode->SetAttribute( "name", block.name.getData() );
			blockNode->SetAttribute( "first", block.first );
			blockNode->SetAttribute( "last", block.last );
		}
	}

	bool Resource::updateImageFileData()
	{
		if( m_image )
		{
			ImAppImageFree( m_imapp, m_image );
			m_image = nullptr;
		}

		spng_ctx* spng = spng_ctx_new( 0 );
		const int bufferResult = spng_set_png_buffer( spng, m_fileData.getData(), m_fileData.getSizeInBytes() );
		if( bufferResult != SPNG_OK )
		{
			ImAppTrace( "Error: Failed to set PNG buffer. Result: %d", bufferResult );
			spng_ctx_free( spng );
			return false;
		}

		size_t pixelDataSize;
		const int sizeResult = spng_decoded_image_size( spng, SPNG_FMT_RGBA8, &pixelDataSize );
		if( sizeResult != SPNG_OK )
		{
			ImAppTrace( "Error: Failed to calculate PNG size. Result: %d", sizeResult );
			spng_ctx_free( spng );
			return false;
		}

		struct spng_ihdr ihdr;
		const int headerResult = spng_get_ihdr( spng, &ihdr );
		if( headerResult != SPNG_OK )
		{
			ImAppTrace( "Error: Failed to get PNG header. Result: %d", headerResult );
			spng_ctx_free( spng );
			return false;
		}

		m_imageData.setLengthUninitialized( pixelDataSize );

		const int decodeResult = spng_decode_image( spng, m_imageData.getData(), m_imageData.getLength(), SPNG_FMT_RGBA8, 0 );
		spng_ctx_free( spng );

		if( decodeResult != 0 )
		{
			ImAppTrace( "Error: Failed to decode PNG. Result: %d", decodeResult );
			return false;
		}

		m_imageWidth	= ihdr.width;
		m_imageHeight	= ihdr.height;

		return true;
	}
}

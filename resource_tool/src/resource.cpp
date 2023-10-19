#include "resource.h"

#include <tiki/tiki_path.h>

#include <spng/spng.h>

namespace imapp
{
	using namespace imui;
	using namespace tiki;

	static constexpr float FileCheckInterval = 2.0f;

	static const StringView s_resourceTypeStrings[] =
	{
		"Image",
		"Skin",
		"Font",
		"Theme"
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
			return m_theme.load( m_xml );
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
			m_theme.serialize( m_xml );
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

	void Resource::updateFileData( ImAppContext* imapp, const Path& packagePath, float time )
	{
		if( (m_type != ResourceType::Image && m_type != ResourceType::Font) ||
			time - m_fileCheckTime < FileCheckInterval )
		{
			return;
		}

		// TODO: check file modified time

		m_fileCheckTime = time;

		const Path imagePath = packagePath.getParent().push( m_fileSourcePath );
		FILE* file = fopen( imagePath.getNativePath().getData(), "rb" );
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

		switch( m_type )
		{
		case ResourceType::Image:
			updateImageFileData( imapp );
			break;

		case ResourceType::Font:
			// ???
			break;

		case ResourceType::Skin:
		case ResourceType::Theme:
		case ResourceType::Count:
			break;
		}

		m_fileHash = newFileHash;
		m_revision++;
	}

	void Resource::setName( const StringView& value )
	{
		m_name = value;
		m_revision++;
	}

	void Resource::setFileSourcePath( const StringView& value )
	{
		m_fileSourcePath = value;
		m_revision++;
	}

	void Resource::setImageAllowAtlas( bool value )
	{
		m_revision += (value != m_imageAllowAtlas);
		m_imageAllowAtlas = value;
	}

	void Resource::setFontSize( float value )
	{
		m_fontSize = value;
		m_revision++;
	}

	void Resource::setSkinImageName( const StringView& value )
	{
		m_skinImageName = value;
		m_revision++;
	}

	void Resource::increaseRevision()
	{
		m_revision++;
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

	bool Resource::loadFontXml()
	{
		return true;
	}

	void Resource::serializeImageXml()
	{
		m_xml->SetAttribute( "path", m_fileSourcePath );
		m_xml->SetAttribute( "allow_atlas", m_imageAllowAtlas );
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
	}

	void Resource::updateImageFileData( ImAppContext* imapp )
	{
		if( m_image )
		{
			ImAppImageFree( imapp, m_image );
			m_image = nullptr;
		}

		spng_ctx* spng = spng_ctx_new( 0 );
		const int bufferResult = spng_set_png_buffer( spng, m_fileData.getData(), m_fileData.getSizeInBytes() );
		if( bufferResult != SPNG_OK )
		{
			//IMAPP_DEBUG_LOGE( "Failed to set PNG buffer. Result: %d", bufferResult );
			spng_ctx_free( spng );
			return;
		}

		size_t pixelDataSize;
		const int sizeResult = spng_decoded_image_size( spng, SPNG_FMT_RGBA8, &pixelDataSize );
		if( sizeResult != SPNG_OK )
		{
			//IMAPP_DEBUG_LOGE( "Failed to calculate PNG size. Result: %d", sizeResult );
			spng_ctx_free( spng );
			return;
		}

		struct spng_ihdr ihdr;
		const int headerResult = spng_get_ihdr( spng, &ihdr );
		if( headerResult != SPNG_OK )
		{
			//IMAPP_DEBUG_LOGE( "Failed to get PNG header. Result: %d", headerResult );
			spng_ctx_free( spng );
			return;
		}

		m_imageData.setLengthUninitialized( pixelDataSize );

		const int decodeResult = spng_decode_image( spng, m_imageData.getData(), m_imageData.getLength(), SPNG_FMT_RGBA8, 0 );
		spng_ctx_free( spng );

		if( decodeResult != 0 )
		{
			//IMAPP_DEBUG_LOGE( "Failed to decode PNG. Result: %d", decodeResult );
			return;
		}

		m_image			= ImAppImageCreateRaw( imapp, m_imageData.getData(), m_imageData.getSizeInBytes(), ihdr.width, ihdr.height );
		m_imageWidth	= ihdr.width;
		m_imageHeight	= ihdr.height;
	}
}

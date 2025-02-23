#include "compiler.h"

#include "resource.h"
#include "resource_package.h"

#include "thread.h"

#include <tiki/tiki_hash_set.h>

#define K15_IA_IMPLEMENTATION
#include <K15_ImageAtlas.h>

namespace imapp
{
	using namespace tiki;

	struct ResourceCompilerResource
	{
		ImAppResPakType	type;

		Resource*		sourceResource;
	};

	Compiler::Compiler()
	{
	}

	Compiler::~Compiler()
	{
		if( m_thread )
		{
			destroyThread( m_thread );
		}
	}

	void Compiler::reset()
	{
		m_resources.clear();
		m_atlasImages.clear();
	}

	bool Compiler::startCompile( const ResourcePackage& package )
	{
		if( isRunning() )
		{
			return false;
		}

		m_packageName = package.getName();
		m_outputPath = package.getPath().getParent().push( package.getOutputPath() );
		m_outputCode = package.getExportCode();

		ResourceMap oldResources;
		oldResources.swap( m_resources );

		for( const Resource* resource : package.getResources() )
		{
			const DynamicString& resourceName = resource->getName();

			CompilerResourceData* resourceData = nullptr;
			if( oldResources.findAndCopy( resourceData, resourceName ) )
			{
				oldResources.remove( resourceName );
				resourceData->applyResource( *resource );
			}
			else
			{
				resourceData = new CompilerResourceData( *resource );
			}

			m_resources.insert( resourceName, resourceData );
		}

		for( ResourceMap::PairType& kvp : oldResources )
		{
			delete kvp.value;
		}

		if( m_thread )
		{
			destroyThread( m_thread );
			m_thread = nullptr;
		}

		m_thread = createThread( "", staticRunCompileThread, this );

		return true;
	}

	void Compiler::waitForCompile()
	{
		if( !m_thread )
		{
			return;
		}

		destroyThread( m_thread );
		m_thread = nullptr;
	}

	bool Compiler::isRunning()
	{
		if( !m_thread )
		{
			return false;
		}

		if( !isThreadRunning( m_thread ) )
		{
			destroyThread( m_thread );
			m_thread = nullptr;

			return false;
		}

		return true;
	}

	/*static*/ void Compiler::staticRunCompileThread( void* arg )
	{
		Compiler* compiler = (Compiler*)arg;
		compiler->runCompileThread();
	}

	void Compiler::runCompileThread()
	{
		m_buffer.clear();
		m_output.clear();

		if( !updateImageAtlas() )
		{
			m_output.pushMessage( CompilerErrorLevel::Error, "Image Atlas", "Failed to update image atlas." );
			return;
		}

		CompiledResourceArray compiledResources;
		ResourceTypeIndexMap resourceIndexMapping;
		ResourceTypeIndexArray resourcesByType;

		prepareCompiledResources( compiledResources, resourceIndexMapping, resourcesByType );

		const uint32 headerOffset = preallocateToBuffer< ImAppResPakHeader >();
		{
			ImAppResPakHeader& bufferHeader = getBufferData< ImAppResPakHeader >( headerOffset );

			const char* magic = IMAPP_RES_PAK_MAGIC;
			memcpy( bufferHeader.magic, magic, sizeof( bufferHeader.magic ) );

			bufferHeader.resourceCount	= (uint16)compiledResources.getLength();
		}

		const uint32 resourcesOffset = preallocateArrayToBuffer< ImAppResPakResource >( compiledResources.getLength() );

		{
			ImAppResPakHeader& bufferHeader = getBufferData< ImAppResPakHeader >( headerOffset );

			for( uintsize i = 0u; i < resourcesByType.getLength(); ++i )
			{
				const DynamicArray< uint16 >& indices = resourcesByType[ i ];

				bufferHeader.resourcesByTypeIndexOffset[ i ]	= writeArrayToBuffer< uint16 >( indices );
				bufferHeader.resourcesbyTypeCount[ i ]			= (uint16)indices.getLength();
			}
		}

		writeResourceNames( compiledResources );
		writeResourceHeaders( resourcesOffset, compiledResources, resourceIndexMapping );

		{
			ImAppResPakHeader& bufferHeader = getBufferData< ImAppResPakHeader >( headerOffset );
			bufferHeader.resourcesOffset = (uint32)m_buffer.getLength();
		}

		writeResourceData( resourcesOffset, compiledResources, resourceIndexMapping );

		if( m_outputCode )
		{
			writeCodeFile();
		}
		else
		{
			writeBinaryFile();
		}
	}

	bool Compiler::updateImageAtlas()
	{
		bool isAtlasUpToDate = false;
		DynamicArray< CompilerResourceData* > images;
		for( ResourceMap::PairType& kvp : m_resources )
		{
			CompilerResourceData& resData = *kvp.value;

			if( !resData.isAtlasImage() )
			{
				continue;
			}

			AtlasImage* atlasImage = m_atlasImages.find( kvp.key );
			if( atlasImage )
			{
				isAtlasUpToDate &= atlasImage->width == resData.getData().image.width;
				isAtlasUpToDate &= atlasImage->height == resData.getData().image.height;
			}
			else
			{
				isAtlasUpToDate = false;
			}

			images.pushBack( &resData );
		}

		if( isAtlasUpToDate )
		{
			return true;
		}

		m_atlasImages.clear();

		CompilerResourceData::ResourceData atlasData;

		if( images.isEmpty() )
		{
			m_atlasData.applyResourceData( atlasData );
			return true;
		}

		K15_ImageAtlas atlas;
		if( K15_IACreateAtlas( &atlas, (kia_u32)images.getLength() ) != K15_IA_RESULT_SUCCESS )
		{
			return false;
		}

		for( CompilerResourceData* image : images )
		{
			ArrayView< uint32 > targetData = image->compileImageAtlasData().cast< uint32 >();
			const CompilerResourceData::ResourceData& resData = image->getData();
			const CompilerResourceData::ImageData& imageData = resData.image;

			int x;
			int y;
			const kia_result imageResult = K15_IAAddImageToAtlas( &atlas, KIA_PIXEL_FORMAT_R8G8B8A8, targetData.getData(), imageData.width + 2u, imageData.height + 2u, &x, &y );
			if( imageResult != K15_IA_RESULT_SUCCESS )
			{
				m_output.pushMessage( CompilerErrorLevel::Error, resData.name, "Failed to add '%s' to atlas. Error: %d", resData.name.getData(), imageResult );
				m_atlasImages.clear();
				return false;
			}

			AtlasImage& atlasImage = m_atlasImages[ resData.name ];
			atlasImage.x		= (uint16)x + 1u;
			atlasImage.y		= (uint16)y + 1u;
			atlasImage.width	= (uint16)imageData.width;
			atlasImage.height	= (uint16)imageData.height;
		}

		const kia_u32 atlasSize = K15_IACalculateAtlasPixelDataSizeInBytes( &atlas, KIA_PIXEL_FORMAT_R8G8B8A8 );

		int width;
		int height;
		atlasData.image.imageData.setLengthUninitialized( atlasSize );
		K15_IABakeImageAtlasIntoPixelBuffer( &atlas, KIA_PIXEL_FORMAT_R8G8B8A8, atlasData.image.imageData.getData(), &width, &height );

		atlasData.name				= "atlas";
		atlasData.type				= ResourceType::Image;
		atlasData.image.width			= (uint16)width;
		atlasData.image.height		= (uint16)height;
		atlasData.image.allowAtlas	= false;

		m_atlasData.applyResourceData( atlasData );
		return true;
	}

	void Compiler::prepareCompiledResources( CompiledResourceArray& compiledResources, ResourceTypeIndexMap& resourceIndexMapping, ResourceTypeIndexArray& resourcesByType )
	{
		if( m_atlasData.getData().image.imageData.hasElements() )
		{
			CompiledResource& compiledResource = compiledResources.pushBack();
			compiledResource.type		= ImAppResPakType_Texture;
			compiledResource.data		= &m_atlasData;
			compiledResource.fontData	= nullptr;

			resourceIndexMapping[ ImAppResPakType_Texture ].insert( m_atlasData.getData().name, 0u );
			resourcesByType[ ImAppResPakType_Texture ].pushBack( 0u );
		}

		for( ResourceMap::PairType& kvp : m_resources )
		{
			CompilerResourceData& resource = *kvp.value;
			const CompilerResourceData::ResourceData& resData = resource.getData();

			uint16 refIndex = 0u; // 0 == texture or unused
			const CompilerFontData* fontData = nullptr;
			if( resData.type == ResourceType::Image &&
				resData.image.imageData.isEmpty() )
			{
				m_output.pushMessage( CompilerErrorLevel::Warning, resData.name, "Image '%s' has no pixel data.", resData.name.getData() );
				continue;
			}

			if( resData.type == ResourceType::Font )
			{
				if( resData.font.isScalable )
				{
					fontData = resource.compileFontSdf( m_output );
				}
				else
				{
					fontData = resource.compileFont( m_output );
				}
			}

			if( (resData.type == ResourceType::Image && !resData.image.allowAtlas) ||
				resData.type == ResourceType::Font )
			{
				refIndex = (uint16)compiledResources.getLength();

				CompiledResource& textureResource = compiledResources.pushBack();
				textureResource.type		= ImAppResPakType_Texture;
				textureResource.data		= &resource;
				textureResource.fontData	= fontData;

				resourceIndexMapping[ ImAppResPakType_Texture ].insert( resData.name, refIndex );
				resourcesByType[ ImAppResPakType_Texture ].pushBack( refIndex );
			}

			ImAppResPakType type = ImAppResPakType_MAX;
			switch( resData.type )
			{
			case ResourceType::Image:	type = ImAppResPakType_Image; break;
			case ResourceType::Skin:	type = ImAppResPakType_Skin; break;
			case ResourceType::Font:	type = ImAppResPakType_Font; break;
			case ResourceType::Theme:	type = ImAppResPakType_Theme; break;
			case ResourceType::Count:	break;
			}

			if( type == ImAppResPakType_MAX )
			{
				continue;
			}

			const uint16 index = (uint16)compiledResources.getLength();
			CompiledResource& compiledResource = compiledResources.pushBack();
			compiledResource.type		= type;
			compiledResource.data		= &resource;
			compiledResource.fontData	= fontData;
			compiledResource.refIndex	= refIndex;

			resourceIndexMapping[ type ].insert( resData.name, index );
			resourcesByType[ type ].pushBack( index );
		}
	}

	void Compiler::writeResourceNames( CompiledResourceArray& compiledResources )
	{
		for( CompiledResource& compiledResource : compiledResources )
		{
			const DynamicString& name = compiledResource.data->getData().name;

			if( name.getLength() > 255u )
			{
				m_output.pushMessage( CompilerErrorLevel::Warning, name, "Resource name too long." );
			}

			compiledResource.nameOffset	= writeArrayToBuffer< char >( name.getRange( 0u, 255u ) );
			compiledResource.nameLength	= (uint8)min< uintsize >( name.getLength(), 255u );

			writeToBuffer< char >( '\0' ); // string null terminator
		}
	}

	void Compiler::writeResourceHeaders( uint32 resourcesOffset, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping )
	{
		for( uintsize compiledResourceIndex = 0u; compiledResourceIndex < compiledResources.getLength(); ++compiledResourceIndex )
		{
			const CompiledResource& compiledResource = compiledResources[ compiledResourceIndex ];
			const CompilerResourceData::ResourceData& data = compiledResource.data->getData();

			{
				ImAppResPakResource& targetResource = getBufferArrayElement< ImAppResPakResource >( resourcesOffset, compiledResourceIndex );

				targetResource.type			= compiledResource.type;
				targetResource.nameOffset	= compiledResource.nameOffset;
				targetResource.nameLength	= compiledResource.nameLength;
				targetResource.headerOffset	= 0u;
				targetResource.headerSize	= 0u;
				targetResource.dataOffset	= 0u;
				targetResource.dataSize		= 0u;
			}

			uint16 textureIndex = IMAPP_RES_PAK_INVALID_INDEX;
			uint32 headerOffset = 0u;
			uint32 headerSize = 0u;
			switch( compiledResource.type )
			{
			case ImAppResPakType_Texture:
				{
					ImAppResPakTextureHeader textureHeader;
					if( !compiledResource.fontData )
					{
						textureHeader.format	= ImAppResPakTextureFormat_RGBA8;
						textureHeader.flags		= data.image.repeat ? ImAppResPakTextureFlags_Repeat : 0u;
						textureHeader.width		= data.image.width;
						textureHeader.height	= data.image.height;
					}
					else
					{
						const CompilerFontData& fontData = *compiledResource.fontData;

						textureHeader.format	= data.font.isScalable ? ImAppResPakTextureFormat_RGB8 : ImAppResPakTextureFormat_A8;
						textureHeader.flags		= ImAppResPakTextureFlags_Font;
						textureHeader.width		= fontData.width;
						textureHeader.height	= fontData.height;
					}

					headerOffset = writeToBuffer( textureHeader );
					headerSize = sizeof( textureHeader );
				}
				break;

			case ImAppResPakType_Image:
				{
					textureIndex = compiledResource.refIndex;

					ImAppResPakImageHeader imageHeader;
					if( data.image.allowAtlas )
					{
						const AtlasImage* atlasImage = m_atlasImages.find( data.name );
						if( !atlasImage )
						{
							m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find '%s' in atlas.", data.name.getData() );
							continue;
						}

						imageHeader.x		= atlasImage->x;
						imageHeader.y		= atlasImage->y;
						imageHeader.width	= atlasImage->width;
						imageHeader.height	= atlasImage->height;
					}
					else
					{
						imageHeader.x		= 0u;
						imageHeader.y		= 0u;
						imageHeader.width	= (uint16)data.image.width;
						imageHeader.height	= (uint16)data.image.height;
					}

					headerOffset = writeToBuffer( imageHeader );
					headerSize = sizeof( imageHeader );
				}
				break;

			case ImAppResPakType_Skin:
				{
					ImAppResPakSkinHeader skinHeader;
					skinHeader.top		= data.skin.border.top;
					skinHeader.left		= data.skin.border.left;
					skinHeader.right	= data.skin.border.right;
					skinHeader.bottom	= data.skin.border.bottom;

					uint16 imageIndex;
					if( !findResourceIndex( imageIndex, resourceIndexMapping, ImAppResPakType_Image, data.skin.imageName, data.name ) ||
						imageIndex == IMAPP_RES_PAK_INVALID_INDEX )
					{
						m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find image '%s' for skin '%s'.", data.skin.imageName.getData(), data.name.getData() );
						continue;
					}

					const CompiledResource& imageResource = compiledResources[ imageIndex ];
					if( imageResource.data->getData().image.allowAtlas )
					{
						const AtlasImage* atlasImage = m_atlasImages.find( imageResource.data->getData().name );
						if( !atlasImage )
						{
							m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find '%s' in atlas.", imageResource.data->getData().name.getData() );
							continue;
						}

						textureIndex = 0u;

						skinHeader.x		= atlasImage->x;
						skinHeader.y		= atlasImage->y;
						skinHeader.width	= atlasImage->width;
						skinHeader.height	= atlasImage->height;
					}
					else
					{
						if( !findResourceIndex( textureIndex, resourceIndexMapping, ImAppResPakType_Texture, data.skin.imageName, data.name ) )
						{
							m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find texture '%s' for skin '%s'.", data.skin.imageName.getData(), data.name.getData() );
							continue;
						}

						skinHeader.x		= 0u;
						skinHeader.y		= 0u;
						skinHeader.width	= (uint16)imageResource.data->getData().image.width;
						skinHeader.height	= (uint16)imageResource.data->getData().image.height;
					}

					headerOffset = writeToBuffer( skinHeader );
					headerSize = sizeof( skinHeader );
				}
				break;

			case ImAppResPakType_Font:
				{
					ImAppResPakFontHeader fontHeader;
					fontHeader.codepointCount	= compiledResource.fontData ? (uint32)compiledResource.fontData->codepoints.getLength() : 0u;
					fontHeader.ttfDataSize		= (uint32)data.fileData.getSizeInBytes();
					fontHeader.fontSize			= data.font.size;
					fontHeader.isScalable		= data.font.isScalable;

					if( !findResourceIndex( textureIndex, resourceIndexMapping, ImAppResPakType_Texture, data.name, data.name ) )
					{
						m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find texture '%s' for skin '%s'.", data.skin.imageName.getData(), data.name.getData() );
						continue;
					}

					headerOffset = writeToBuffer( fontHeader );
					headerSize = sizeof( fontHeader );
				}
				break;

			case ImAppResPakType_Theme:
				{
					ImAppResPakThemeHeader themeHeader;

					const ResourceTheme& theme = *data.theme.theme;
					const UiToolboxConfig& config = theme.getConfig();

					HashSet< uint16 > referencesSet;
					if( !findResourceIndex( themeHeader.fontIndex, resourceIndexMapping, ImAppResPakType_Font, theme.getFontName(), data.name ) )
					{
						m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find font '%s' in theme '%s'.", theme.getFontName().getData(), data.name.getData() );
					}
					referencesSet.insert( themeHeader.fontIndex );

					for( uintsize i = 0u; i < TIKI_ARRAY_COUNT( themeHeader.colors ); ++i )
					{
						themeHeader.colors[ i ] = config.colors[ i ];
					}

					for( uintsize i = 0u; i < TIKI_ARRAY_COUNT( themeHeader.skinIndices ); ++i )
					{
						const StringView skinName = theme.getSkinName( (ImUiToolboxSkin)i );
						if( !findResourceIndex( themeHeader.skinIndices[ i ], resourceIndexMapping, ImAppResPakType_Skin, skinName, data.name ) )
						{
							m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find skin '%s' for slot %d in theme '%s'.", skinName.getData(), i, data.name.getData() );
						}
						referencesSet.insert( themeHeader.skinIndices[ i ] );
					}

					for( uintsize i = 0u; i < TIKI_ARRAY_COUNT( themeHeader.iconIndices ); ++i )
					{
						const StringView iconName = theme.getIconName( (ImUiToolboxIcon)i );
						if( !findResourceIndex( themeHeader.iconIndices[ i ], resourceIndexMapping, ImAppResPakType_Image, iconName, data.name ) )
						{
							m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find image '%s' for slot %d in theme '%s'.", iconName.getData(), i, data.name.getData() );
						}
						referencesSet.insert( themeHeader.iconIndices[ i ] );
					}

					DynamicArray< uint16 > references;
					for( uint16 resIndex : referencesSet )
					{
						if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
						{
							continue;
						}

						references.pushBack( resIndex );
					}
					themeHeader.referencedCount	= (uint16_t)references.getLength();

					themeHeader.button			= config.button;
					themeHeader.checkBox		= config.checkBox;
					themeHeader.slider			= config.slider;
					themeHeader.textEdit		= config.textEdit;
					themeHeader.progressBar		= config.progressBar;
					themeHeader.scrollArea		= config.scrollArea;
					themeHeader.list			= config.list;
					themeHeader.dropDown		= config.dropDown;
					themeHeader.popup			= config.popup;

					headerOffset = writeToBuffer( themeHeader );
					headerSize = sizeof( themeHeader ) + (uint32)references.getSizeInBytes();

					writeArrayToBuffer< uint16 >( references );
				}
				break;

			case ImAppResPakType_MAX:
				break;
			}

			{
				ImAppResPakResource& targetResource = getBufferArrayElement< ImAppResPakResource >( resourcesOffset, compiledResourceIndex );

				targetResource.textureIndex	= textureIndex;
				targetResource.headerOffset	= headerOffset;
				targetResource.headerSize	= headerSize;
			}
		}
	}

	void Compiler::writeResourceData( uint32 resourcesOffset, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping )
	{
		for( uintsize compiledResourceIndex = 0u; compiledResourceIndex < compiledResources.getLength(); ++compiledResourceIndex )
		{
			const CompiledResource& compiledResource = compiledResources[ compiledResourceIndex ];
			const CompilerResourceData::ResourceData& data = compiledResource.data->getData();

			uint32 dataOffset = 0u;
			uint32 dataSize = 0u;
			switch( compiledResource.type )
			{
			case ImAppResPakType_Texture:
				if( !compiledResource.fontData )
				{
					dataOffset	= writeArrayToBuffer< byte >( data.image.imageData );
					dataSize	= (uint32)data.image.imageData.getSizeInBytes();
				}
				else
				{
					const CompilerFontData& fontData = *compiledResource.fontData;

					dataOffset	= writeArrayToBuffer< byte >( fontData.pixelData );
					dataSize	= (uint32)fontData.pixelData.getSizeInBytes();
				}
				break;

			case ImAppResPakType_Image:
				break;

			case ImAppResPakType_Skin:
				break;

			case ImAppResPakType_Font:
				if( compiledResource.fontData )
				{
					const CompilerFontData& fontData = *compiledResource.fontData;

					dataOffset	= writeArrayToBuffer( fontData.codepoints );
					dataSize	= (uint32)fontData.codepoints.getSizeInBytes();

					writeArrayToBuffer( data.fileData );
					dataSize += (uint32)data.fileData.getSizeInBytes();
				}
				break;

			case ImAppResPakType_Theme:
				break;

			case ImAppResPakType_Blob:
				dataOffset	= writeArrayToBuffer< byte >( data.fileData );
				dataSize	= (uint32)data.fileData.getSizeInBytes();
				break;

			case ImAppResPakType_MAX:
				break;
			}

			{
				ImAppResPakResource& targetResource = getBufferArrayElement< ImAppResPakResource >( resourcesOffset, compiledResourceIndex );

				targetResource.dataOffset	= dataOffset;
				targetResource.dataSize		= dataSize;
			}
		}
	}

	template< typename T >
	T& Compiler::getBufferData( uint32 offset )
	{
		return *(T*)&m_buffer[ offset ];
	}

	template< typename T >
	T& Compiler::getBufferArrayElement( uint32 offset, uintsize index )
	{
		return *(T*)&m_buffer[ offset + (sizeof( T ) * index) ];
	}

	template< typename T >
	uint32 Compiler::preallocateToBuffer( uintsize alignment /*= 1u */ )
	{
		return preallocateArrayToBuffer< T >( alignment );
	}

	template< typename T >
	uint32 Compiler::preallocateArrayToBuffer( uintsize length, uintsize alignment /* = 1u */ )
	{
		const uintsize alignedLength = alignValue( m_buffer.getLength(), alignment );
		m_buffer.setLengthZero( alignedLength );

		m_buffer.pushRange( sizeof( T ) * length );
		return (uint32)alignedLength;
	}

	template< typename T >
	uint32 Compiler::writeToBuffer( const T& value, uintsize alignment /* = 1u */ )
	{
		const uint32 offset = preallocateToBuffer< T >( alignment );
		getBufferData< T >( offset ) = value;
		return offset;
	}

	template< typename T >
	uint32 Compiler::writeArrayToBuffer( const ArrayView< T >& array, uintsize alignment /* = 1u */ )
	{
		const uintsize alignedLength = alignValue( m_buffer.getLength(), alignment );
		m_buffer.setLengthZero( alignedLength );

		m_buffer.pushRange( (const byte*)array.getData(), array.getSizeInBytes() );
		return (uint32)alignedLength;
	}

	void Compiler::writeBinaryFile()
	{
		const Path binaryPath = m_outputPath.addExtension( ".iarespak" );

		FILE* file = fopen( binaryPath.getNativePath().getData(), "wb" );
		if( !file )
		{
			m_output.pushMessage( CompilerErrorLevel::Error, "package", "Failed to open '%s'.", m_outputPath.getGenericPath().getData() );
			return;
		}

		fwrite( m_buffer.getData(), m_buffer.getLength(), 1u, file );
		fclose( file );
	}

	void Compiler::writeCodeFile()
	{
		const Path hPath = m_outputPath.addExtension( ".h" );
		//const Path cPath = m_outputPath.addExtension( ".c" );

		//const DynamicString cPath = codePath.getNativePath();
		FILE* file = fopen( hPath.getNativePath().toConstCharPointer(), "w" );
		if( !file )
		{
			m_output.pushMessage( CompilerErrorLevel::Error, "Package", "Failed to open '%s'\n", hPath.getGenericPath().toConstCharPointer() );
			return;
		}

		DynamicString varName = m_packageName;
		varName = varName.replace( ' ', '_' );
		varName = varName.replace( '.', '_' );
		varName = "ImAppResPak" + varName;

		DynamicString content = DynamicString::format( "#pragma once\n\nstatic const unsigned char %s[] =\n{\n\t", varName.toConstCharPointer() );
		content.reserve( 256u + (m_buffer.getLength() * 6) );
		content.terminate( content.getLength() );

		static const char s_hexChars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

		char buffer[ 5u ] = "0x00";
		uintsize lineLength = 0u;
		for( uintsize i = 0u; i < m_buffer.getLength(); ++i )
		{
			const uint8 b = m_buffer[ i ];
			if( lineLength == 16u )
			{
				content += ",\n\t";
				lineLength = 0u;
			}
			else if( lineLength != 0u )
			{
				content += ", ";
			}

			const uint8 highNibble = (b >> 4u) & 0xf;
			const uint8 lowNibble = b & 0xf;
			buffer[ 2u ] = s_hexChars[ highNibble ];
			buffer[ 3u ] = s_hexChars[ lowNibble ];

			content += buffer;

			lineLength++;
		}

		content += "\n};\n";

		fwrite( content.getData(), content.getLength(), 1u, file );
		fclose( file );
	}

	bool Compiler::findResourceIndex( uint16& target, const ResourceTypeIndexMap& mapping, ImAppResPakType type, const DynamicString& name, const StringView& resourceName ) const
	{
		if( name.isEmpty() )
		{
			target = IMAPP_RES_PAK_INVALID_INDEX;
			return true;
		}

		const uint16* resourceIndex = mapping[ type ].find( name );
		if( !resourceIndex )
		{
			target = IMAPP_RES_PAK_INVALID_INDEX;
			return false;
		}

		target = *resourceIndex;
		return true;
	}
}

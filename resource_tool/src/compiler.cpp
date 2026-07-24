#include "compiler.h"

#include "resource.h"
#include "resource_package.h"
#include "resource_theme.h"

#include "thread.h"

#include <tiki/tiki_hash_set.h>

#define K15_IA_IMPLEMENTATION
#include <K15_ImageAtlas.h>

namespace imapp
{
	using namespace tiki;

	struct ResourceCompilerResource
	{
		ImappResPakType	type;

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

		const uint32 headerOffset = m_buffer.preallocateToBuffer< ImappResPakHeader >();
		{
			ImappResPakHeader& bufferHeader = m_buffer.getBufferData< ImappResPakHeader >( headerOffset );

			const char* magic = IMAPP_RES_PAK_MAGIC;
			memcpy( bufferHeader.magic, magic, sizeof( bufferHeader.magic ) );

			bufferHeader.resourceCount	= (uint16)compiledResources.getLength();
		}

		const uint32 resourcesOffset = m_buffer.preallocateArrayToBuffer< ImappResPakResource >( compiledResources.getLength() );

		{
			ImappResPakHeader& bufferHeader = m_buffer.getBufferData< ImappResPakHeader >( headerOffset );

			for( uintsize i = 0u; i < resourcesByType.getLength(); ++i )
			{
				const DynamicArray< uint16 >& indices = resourcesByType[ i ];

				bufferHeader.resourcesByTypeIndexOffset[ i ]	= m_buffer.writeArrayToBuffer< uint16 >( indices );
				bufferHeader.resourcesbyTypeCount[ i ]			= (uint16)indices.getLength();
			}
		}

		writeResourceNames( compiledResources );
		writeResourceHeaders( resourcesOffset, compiledResources, resourceIndexMapping );

		{
			ImappResPakHeader& bufferHeader = m_buffer.getBufferData< ImappResPakHeader >( headerOffset );
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
		atlasData.image.width		= (uint16)width;
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
			compiledResource.type		= ImappResPakType_Texture;
			compiledResource.data		= &m_atlasData;
			compiledResource.fontData	= nullptr;

			resourceIndexMapping[ ImappResPakType_Texture ].insert( m_atlasData.getData().name, 0u );
			resourcesByType[ ImappResPakType_Texture ].pushBack( 0u );
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
				textureResource.type		= ImappResPakType_Texture;
				textureResource.data		= &resource;
				textureResource.fontData	= fontData;

				resourceIndexMapping[ ImappResPakType_Texture ].insert( resData.name, refIndex );
				resourcesByType[ ImappResPakType_Texture ].pushBack( refIndex );
			}

			ImappResPakType type = ImappResPakType_MAX;
			switch( resData.type )
			{
			case ResourceType::Image:	type = ImappResPakType_Image; break;
			case ResourceType::Skin:	type = ImappResPakType_Skin; break;
			case ResourceType::Font:	type = ImappResPakType_Font; break;
			case ResourceType::Theme:	type = ImappResPakType_Theme; break;
			case ResourceType::Count:	break;
			}

			if( type == ImappResPakType_MAX )
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

			compiledResource.nameOffset	= m_buffer.writeArrayToBuffer< char >( name.getRange( 0u, 255u ) );
			compiledResource.nameLength	= (uint8)min< uintsize >( name.getLength(), 255u );

			m_buffer.writeToBuffer< char >( '\0' ); // string null terminator
		}
	}

	void Compiler::writeResourceHeaders( uint32 resourcesOffset, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping )
	{
		for( uintsize compiledResourceIndex = 0u; compiledResourceIndex < compiledResources.getLength(); ++compiledResourceIndex )
		{
			const CompiledResource& compiledResource = compiledResources[ compiledResourceIndex ];
			const CompilerResourceData::ResourceData& data = compiledResource.data->getData();

			{
				ImappResPakResource& targetResource = m_buffer.getBufferArrayElement< ImappResPakResource >( resourcesOffset, compiledResourceIndex );

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
			case ImappResPakType_Texture:
				{
					ImappResPakTextureHeader textureHeader;
					if( !compiledResource.fontData )
					{
						textureHeader.format	= ImappResPakTextureFormat_RGBA8;
						textureHeader.flags		= data.image.repeat ? ImappResPakTextureFlags_Repeat : 0u;
						textureHeader.width		= data.image.width;
						textureHeader.height	= data.image.height;
					}
					else
					{
						const CompilerFontData& fontData = *compiledResource.fontData;

						textureHeader.format	= data.font.isScalable ? ImappResPakTextureFormat_RGBA8 : ImappResPakTextureFormat_A8;
						textureHeader.flags		= data.font.isScalable ? ImappResPakTextureFlags_FontSdf : ImappResPakTextureFlags_Font;
						textureHeader.width		= fontData.width;
						textureHeader.height	= fontData.height;
					}

					headerOffset = m_buffer.writeToBuffer( textureHeader );
					headerSize = sizeof( textureHeader );
				}
				break;

			case ImappResPakType_Image:
				{
					textureIndex = compiledResource.refIndex;

					ImappResPakImageHeader imageHeader;
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

					headerOffset = m_buffer.writeToBuffer( imageHeader );
					headerSize = sizeof( imageHeader );
				}
				break;

			case ImappResPakType_Skin:
				{
					ImappResPakSkinHeader skinHeader;
					skinHeader.top		= data.skin.border.top;
					skinHeader.left		= data.skin.border.left;
					skinHeader.right	= data.skin.border.right;
					skinHeader.bottom	= data.skin.border.bottom;

					uint16 imageIndex;
					if( !findResourceIndex( imageIndex, resourceIndexMapping, ImappResPakType_Image, data.skin.imageName, data.name ) ||
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
						if( !findResourceIndex( textureIndex, resourceIndexMapping, ImappResPakType_Texture, data.skin.imageName, data.name ) )
						{
							m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find texture '%s' for skin '%s'.", data.skin.imageName.getData(), data.name.getData() );
							continue;
						}

						skinHeader.x		= 0u;
						skinHeader.y		= 0u;
						skinHeader.width	= (uint16)imageResource.data->getData().image.width;
						skinHeader.height	= (uint16)imageResource.data->getData().image.height;
					}

					headerOffset = m_buffer.writeToBuffer( skinHeader );
					headerSize = sizeof( skinHeader );
				}
				break;

			case ImappResPakType_Font:
				{
					ImappResPakFontHeader fontHeader;
					fontHeader.codepointCount	= compiledResource.fontData ? (uint32)compiledResource.fontData->codepoints.getLength() : 0u;
					fontHeader.ttfDataSize		= (uint32)data.fileData.getSizeInBytes();
					fontHeader.fontSize			= data.font.size;
					fontHeader.isScalable		= data.font.isScalable;

					if( !findResourceIndex( textureIndex, resourceIndexMapping, ImappResPakType_Texture, data.name, data.name ) )
					{
						m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find texture '%s' for skin '%s'.", data.skin.imageName.getData(), data.name.getData() );
						continue;
					}

					headerOffset = m_buffer.writeToBuffer( fontHeader );
					headerSize = sizeof( fontHeader );
				}
				break;

			case ImappResPakType_Theme:
				{
					const ResourceTheme& theme = *data.theme.theme;
					const ResourceTheme::ConstFieldView fields = theme.getFields();

					const ImuiToolboxThemeReflection themeReflection = imuiToolboxThemeReflectionGet();

					uintsize fieldCount = 0u;
					for( const ResourceThemeField& field : fields )
					{
						if( !field.uiField )
						{
							continue;
						}

						fieldCount++;
					}

					BinaryBuffer dataBuffer;
					const uint32 fieldsOffset = dataBuffer.preallocateArrayToBuffer< ImappResPakThemeField >( fieldCount );

					uintsize fieldIndex = 0u;
					HashSet< uint16 > referencesSet;
					for( const ResourceThemeField& field : fields )
					{
						if( !field.uiField )
						{
							continue;
						}

						ImappResPakThemeField& fieldData = dataBuffer.getBufferArrayElement< ImappResPakThemeField >( fieldsOffset, fieldIndex );
						fieldData.nameHash	= imuiHashCreate( field.uiField->name, strlen( field.uiField->name ) );
						fieldData.type		= field.uiField->type;
						fieldData.base		= field.base;

						switch( field.uiField->type )
						{
						case ImuiToolboxThemeReflectionType_Color:
							{
								const ImuiColor& data = theme.getFieldColor( field );
								dataBuffer.writeToBuffer( data );
							}
							break;

						case ImuiToolboxThemeReflectionType_Skin:
						case ImuiToolboxThemeReflectionType_Image:
						case ImuiToolboxThemeReflectionType_Font:
							{
								ImappResPakType fieldResType = ImappResPakType_MAX;
								const char* fieldResTypeName = "resource";
								switch( field.uiField->type )
								{
								case ImuiToolboxThemeReflectionType_Skin:
									fieldResType = ImappResPakType_Skin;
									fieldResTypeName = "skin";
									break;

								case ImuiToolboxThemeReflectionType_Image:
									fieldResType = ImappResPakType_Image;
									fieldResTypeName = "image";
									break;

								case ImuiToolboxThemeReflectionType_Font:
									fieldResType = ImappResPakType_Font;
									fieldResTypeName = "font";
									break;
								}

								uint16 resIndex;
								const DynamicString& resName = theme.getFieldString( field );
								if( !findResourceIndex( resIndex, resourceIndexMapping, fieldResType, resName, data.name ) )
								{
									m_output.pushMessage( CompilerErrorLevel::Error, data.name, "Could not find %s '%s' in theme '%s'.", fieldResTypeName, resName, data.name.getData() );
								}
								referencesSet.insert( resIndex );
								dataBuffer.writeToBuffer( resIndex );
							}
							break;

						case ImuiToolboxThemeReflectionType_Size:
							{
								const ImuiSize& data = theme.getFieldSize( field );
								dataBuffer.writeToBuffer( data );
							}
							break;

						case ImuiToolboxThemeReflectionType_Border:
							{
								const ImuiBorder& data = theme.getFieldBorder( field );
								dataBuffer.writeToBuffer( data );
							}
							break;

						case ImuiToolboxThemeReflectionType_Float:
							{
								const float& data = theme.getFieldFloat( field );
								dataBuffer.writeToBuffer( data );
							}
							break;

						case ImuiToolboxThemeReflectionType_Double:
							{
								const double& data = theme.getFieldDouble( field );
								dataBuffer.writeToBuffer( data );
							}
							break;

						case ImuiToolboxThemeReflectionType_UInt32:
							{
								const uint32& data = theme.getFieldUInt32( field );
								dataBuffer.writeToBuffer( data );
							}
							break;
						}

						fieldIndex++;
					}

					BinaryBuffer headerBuffer;

					const uint32 themeHeaderOffset = headerBuffer.preallocateToBuffer< ImappResPakThemeHeader >();

					DynamicArray< uint16 > references;
					for( uint16 resIndex : referencesSet )
					{
						if( resIndex == IMAPP_RES_PAK_INVALID_INDEX )
						{
							continue;
						}

						references.pushBack( resIndex );
					}

					{
						ImappResPakThemeHeader& themeHeader = headerBuffer.getBufferData< ImappResPakThemeHeader >( themeHeaderOffset );
						themeHeader.referencedCount		= (uint16)references.getLength();
						themeHeader.themeFieldCount		= (uint16)fieldCount;
					}

					headerBuffer.writeArrayToBuffer< uint16 >( references );
					headerBuffer.writeArrayToBuffer( dataBuffer.getData() );

					headerOffset = m_buffer.writeArrayToBuffer( headerBuffer.getData() );
					headerSize = (uint32)headerBuffer.getLength();
				}
				break;

			case ImappResPakType_MAX:
				break;
			}

			{
				ImappResPakResource& targetResource = m_buffer.getBufferArrayElement< ImappResPakResource >( resourcesOffset, compiledResourceIndex );

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
			case ImappResPakType_Texture:
				if( !compiledResource.fontData )
				{
					dataOffset	= m_buffer.writeArrayToBuffer< byte >( data.image.imageData );
					dataSize	= (uint32)data.image.imageData.getSizeInBytes();
				}
				else
				{
					const CompilerFontData& fontData = *compiledResource.fontData;

					dataOffset	= m_buffer.writeArrayToBuffer< byte >( fontData.pixelData );
					dataSize	= (uint32)fontData.pixelData.getSizeInBytes();
				}
				break;

			case ImappResPakType_Image:
				break;

			case ImappResPakType_Skin:
				break;

			case ImappResPakType_Font:
				if( compiledResource.fontData )
				{
					const CompilerFontData& fontData = *compiledResource.fontData;

					dataOffset	= m_buffer.writeArrayToBuffer( fontData.codepoints );
					dataSize	= (uint32)fontData.codepoints.getSizeInBytes();

					m_buffer.writeArrayToBuffer( data.fileData );
					dataSize += (uint32)data.fileData.getSizeInBytes();
				}
				break;

			case ImappResPakType_Theme:
				break;

			case ImappResPakType_Blob:
				dataOffset	= m_buffer.writeArrayToBuffer< byte >( data.fileData );
				dataSize	= (uint32)data.fileData.getSizeInBytes();
				break;

			case ImappResPakType_MAX:
				break;
			}

			{
				ImappResPakResource& targetResource = m_buffer.getBufferArrayElement< ImappResPakResource >( resourcesOffset, compiledResourceIndex );

				targetResource.dataOffset	= dataOffset;
				targetResource.dataSize		= dataSize;
			}
		}
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

		fwrite( m_buffer.getData().getData(), m_buffer.getLength(), 1u, file );
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
		varName = "imappResPak" + varName;

		DynamicString content = DynamicString::format( "#pragma once\n\nstatic const unsigned char %s[] =\n{\n\t", varName.toConstCharPointer() );
		content.reserve( 256u + (m_buffer.getLength() * 6) );
		content.terminate( content.getLength() );

		static const char s_hexChars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

		char buffer[ 5u ] = "0x00";
		uintsize lineLength = 0u;
		const BinaryBuffer::ByteView bytes = m_buffer.getData();
		for( uintsize i = 0u; i < bytes.getLength(); ++i )
		{
			const uint8 b = bytes[ i ];
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

	bool Compiler::findResourceIndex( uint16& target, const ResourceTypeIndexMap& mapping, ImappResPakType type, const DynamicString& name, const StringView& resourceName ) const
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

	void Compiler::BinaryBuffer::clear()
	{
		m_buffer.clear();
	}

	template< typename T >
	T& Compiler::BinaryBuffer::getBufferData( uint32 offset )
	{
		return *(T*)&m_buffer[ offset ];
	}

	template< typename T >
	T& Compiler::BinaryBuffer::getBufferArrayElement( uint32 offset, uintsize index )
	{
		return *(T*)&m_buffer[ offset + (sizeof( T ) * index) ];
	}

	template< typename T >
	uint32 Compiler::BinaryBuffer::preallocateToBuffer( uintsize alignment /*= 1u */ )
	{
		return preallocateArrayToBuffer< T >( alignment );
	}

	template< typename T >
	uint32 Compiler::BinaryBuffer::preallocateArrayToBuffer( uintsize length, uintsize alignment /* = 1u */ )
	{
		const uintsize alignedLength = alignValue( m_buffer.getLength(), alignment );
		m_buffer.setLengthZero( alignedLength );

		m_buffer.pushRange( sizeof( T ) * length );
		return (uint32)alignedLength;
	}

	template< typename T >
	uint32 Compiler::BinaryBuffer::writeToBuffer( const T& value, uintsize alignment /* = 1u */ )
	{
		const uint32 offset = preallocateToBuffer< T >( alignment );
		getBufferData< T >( offset ) = value;
		return offset;
	}

	template< typename T >
	uint32 Compiler::BinaryBuffer::writeArrayToBuffer( const ArrayView< T >& array, uintsize alignment /* = 1u */ )
	{
		const uintsize alignedLength = alignValue( m_buffer.getLength(), alignment );
		m_buffer.setLengthZero( alignedLength );

		m_buffer.pushRange( (const byte*)array.getData(), array.getSizeInBytes() );
		return (uint32)alignedLength;
	}


}

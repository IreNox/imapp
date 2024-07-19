#include "resource_compiler.h"

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

	ResourceCompiler::ResourceCompiler()
	{
	}

	ResourceCompiler::~ResourceCompiler()
	{
		if( m_thread )
		{
			destroyThread( m_thread );
		}
	}

	bool ResourceCompiler::startCompile( const ResourcePackage& package )
	{
		if( isRunning() )
		{
			return false;
		}

		m_outputPath = package.getPath().getParent().push( package.getOutputPath() );

		//HashSet< DynamicString > removedResources;
		//for( ResourceMap::ConstIterator it : m_resources )
		//{
		//	removedResources.
		//}

		for( const Resource* resource : package.getResources() )
		{
			ResourceData& data = m_resources[ resource->getName() ];
			_CrtCheckMemory();

			data.type = resource->getType();
			data.name = resource->getName();

			if( (data.type == ResourceType::Image || data.type == ResourceType::Font) &&
				resource->getFileHash() != data.fileHash )
			{
				data.fileData = resource->getFileData();
				data.fileHash = resource->getFileHash();

				if( data.type == ResourceType::Image )
				{
					data.image.imageData = resource->getImageData();
				}
			}

			switch( resource->getType() )
			{
			case ResourceType::Image:
				data.image.width		= resource->getImageWidth();
				data.image.height		= resource->getImageHeight();
				data.image.allowAtlas	= resource->getImageAllowAtlas();
				data.image.repeat		= resource->getImageRepeat();
				break;

			case ResourceType::Font:
				data.font.size			= resource->getFontSize();
				break;

			case ResourceType::Skin:
				data.skin.imageName		= resource->getSkinImageName();
				data.skin.border		= resource->getSkinBorder();
				break;

			case ResourceType::Theme:
				if( data.theme.theme )
				{
					*data.theme.theme = resource->getTheme();
				}
				else
				{
					data.theme.theme = new ResourceTheme( resource->getTheme() );
				}
				break;

			default:
				break;
			}

			_CrtCheckMemory();
			//removedResources.remove();
		}

		//for( unusded )

		if( m_thread )
		{
			destroyThread( m_thread );
			m_thread = nullptr;
		}

		m_thread = createThread( "", staticRunCompileThread, this );

		return true;
	}

	bool ResourceCompiler::isRunning()
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

	/*static*/ void ResourceCompiler::staticRunCompileThread( void* arg )
	{
		ResourceCompiler* compiler = (ResourceCompiler*)arg;
		compiler->runCompileThread();
	}

	void ResourceCompiler::runCompileThread()
	{
		m_result = true;
		m_buffer.clear();
		m_outputs.clear();

		if( !updateImageAtlas() )
		{
			m_result = false;
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

		//mkdir( m_outputPath.getParent().getNativePath().getData() );

		FILE* file = fopen( m_outputPath.getNativePath().getData(), "wb" );
		if( !file )
		{
			pushOutput( ResourceErrorLevel::Error, "package", "Failed to open '%s'.", m_outputPath.getGenericPath().getData() );
			return;
		}

		fwrite( m_buffer.getData(), m_buffer.getLength(), 1u, file );
		fclose( file );
	}

	bool ResourceCompiler::updateImageAtlas()
	{
		// TODO
		bool isAtlasUpToDate = false;
		DynamicArray< ResourceData* > images;
		for( ResourceMap::PairType& kvp : m_resources )
		{
			ResourceData& resData = kvp.value;

			if( resData.type != ResourceType::Image ||
				!resData.image.allowAtlas ||
				resData.image.imageData.isEmpty() )
			{
				continue;
			}

			AtlasImage* atlasImage = m_atlasImages.find( kvp.key );
			if( atlasImage )
			{
				isAtlasUpToDate &= atlasImage->width == resData.image.width;
				isAtlasUpToDate &= atlasImage->height == resData.image.height;
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
		m_atlasData.image.imageData.clear();

		if( images.isEmpty() )
		{
			m_atlasData.image.width		= 0u;
			m_atlasData.image.height	= 0u;
			return true;
		}

		K15_ImageAtlas atlas;
		if( K15_IACreateAtlas( &atlas, (kia_u32)images.getLength() ) != K15_IA_RESULT_SUCCESS )
		{
			return false;
		}

		for( ResourceData* image : images )
		{
			const ArrayView< uint32 > sourceData = image->image.imageData.cast< uint32 >();

			const uintsize sourceLineSize		= image->image.width;
			const uintsize sourceImageSize		= sourceLineSize * image->image.height;
			const uintsize targetLineSize		= image->image.width + 2u;
			const uintsize targetImageSize		= targetLineSize * (image->image.height + 2u);

			image->compiledData.clear();
			image->compiledData.setLengthUninitialized( targetImageSize * 4u );
			Array< uint32 > targetData = image->compiledData.cast< uint32 >();

			// <= to compensate start at 1
			for( uintsize y = 1u; y <= image->image.height; ++y )
			{
				const uint32* sourceLineData	= &sourceData[ (y - 1u) * sourceLineSize ];
				uint32* targetLineData			= &targetData[ y * targetLineSize ];

				memcpy( targetLineData + 1u, sourceLineData, sourceLineSize * sizeof( uint32 ) );

				// fill first and list line border pixels
				targetLineData[ 0u ] = sourceLineData[ 0u ];
				targetLineData[ 1u + sourceLineSize ] = sourceLineData[ sourceLineSize - 1u ];
			}

			{
				const uintsize souceLastLineOffset = sourceImageSize - sourceLineSize;
				const uintsize targetLastLineOffset = (targetImageSize + 1u) - targetLineSize;

				// fill border first and last line
				memcpy( &targetData[ 1u ], sourceData.getData(), sourceLineSize * sizeof( uint32 ) );
				memcpy( &targetData[ targetLastLineOffset ], &sourceData[ souceLastLineOffset ], sourceLineSize * sizeof( uint32 ) );

				// fill border corner pixels
				targetData[ 0u ]								= sourceData[ 0u ];									// TL
				targetData[ targetLineSize - 1u ]				= sourceData[ sourceLineSize - 1u ];				// TR
				targetData[ targetImageSize - targetLineSize ]	= sourceData[ sourceImageSize - sourceLineSize ];	// BL
				targetData[ targetImageSize - 1u ]				= sourceData[ sourceImageSize - 1u ];				// BR
			}

			int x;
			int y;
			const kia_result imageResult = K15_IAAddImageToAtlas( &atlas, KIA_PIXEL_FORMAT_R8G8B8A8, targetData.getData(), image->image.width + 2u, image->image.height + 2u, &x, &y );
			if( imageResult != K15_IA_RESULT_SUCCESS )
			{
				pushOutput( ResourceErrorLevel::Error, image->name, "Failed to add '%s' to atlas. Error: %d", image->name.getData(), imageResult );
				m_atlasImages.clear();
				return false;
			}

			AtlasImage& atlasImage = m_atlasImages[ image->name ];
			atlasImage.x		= (uint16)x + 1u;
			atlasImage.y		= (uint16)y + 1u;
			atlasImage.width	= (uint16)image->image.width;
			atlasImage.height	= (uint16)image->image.height;
		}

		const kia_u32 atlasSize = K15_IACalculateAtlasPixelDataSizeInBytes( &atlas, KIA_PIXEL_FORMAT_R8G8B8A8 );

		int width;
		int height;
		m_atlasData.image.imageData.setLengthUninitialized( atlasSize );
		K15_IABakeImageAtlasIntoPixelBuffer( &atlas, KIA_PIXEL_FORMAT_R8G8B8A8, m_atlasData.image.imageData.getData(), &width, &height );

		m_atlasData.name				= "atlas";
		m_atlasData.type				= ResourceType::Image;
		m_atlasData.image.width			= (uint16)width;
		m_atlasData.image.height		= (uint16)height;
		m_atlasData.image.allowAtlas	= false;

		return true;
	}

	void ResourceCompiler::prepareCompiledResources( CompiledResourceArray& compiledResources, ResourceTypeIndexMap& resourceIndexMapping, ResourceTypeIndexArray& resourcesByType )
	{
		if( m_atlasData.image.imageData.hasElements() )
		{
			CompiledResource& compiledResource = compiledResources.pushBack();
			compiledResource.type = ImAppResPakType_Texture;
			compiledResource.data = &m_atlasData;

			resourceIndexMapping[ ImAppResPakType_Texture ].insert( m_atlasData.name, 0u );
			resourcesByType[ ImAppResPakType_Texture ].pushBack( 0u );
		}

		for( const ResourceMap::PairType& kvp : m_resources )
		{
			const ResourceData& resource = kvp.value;

			uint16 refIndex = 0u; // 0 == texture or unused
			if( resource.type == ResourceType::Image &&
				resource.image.imageData.isEmpty() )
			{
				pushOutput( ResourceErrorLevel::Warning, resource.name, "Image '%s' has no pixel data.", resource.name.getData() );
				continue;
			}

			if( (resource.type == ResourceType::Image && !resource.image.allowAtlas) ||
				resource.type == ResourceType::Font )
			{
				refIndex = (uint16)compiledResources.getLength();

				CompiledResource& textureResource = compiledResources.pushBack();
				textureResource.type		= ImAppResPakType_Texture;
				textureResource.data		= &resource;

				resourceIndexMapping[ ImAppResPakType_Texture ].insert( resource.name, refIndex );
				resourcesByType[ ImAppResPakType_Texture ].pushBack( refIndex );
			}

			ImAppResPakType type = ImAppResPakType_MAX;
			switch( resource.type )
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
			compiledResource.refIndex	= refIndex;

			resourceIndexMapping[ type ].insert( resource.name, index );
			resourcesByType[ type ].pushBack( index );
		}
	}

	void ResourceCompiler::writeResourceNames( CompiledResourceArray& compiledResources )
	{
		for( CompiledResource& compiledResource : compiledResources )
		{
			if( compiledResource.data->name.getLength() > 255u )
			{
				pushOutput( ResourceErrorLevel::Warning, compiledResource.data->name, "Resource name too long." );
			}

			compiledResource.nameOffset	= writeArrayToBuffer< char >( compiledResource.data->name.getRangeView( 0u, 255u ) );
			compiledResource.nameLength	= (uint8)min< uintsize >( compiledResource.data->name.getLength(), 255u );

			writeToBuffer< char >( '\0' ); // string null terminator
		}
	}

	void ResourceCompiler::writeResourceHeaders( uint32 resourcesOffset, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping )
	{
		for( uintsize compiledResourceIndex = 0u; compiledResourceIndex < compiledResources.getLength(); ++compiledResourceIndex )
		{
			const CompiledResource& compiledResource = compiledResources[ compiledResourceIndex ];
			const ResourceData& data = *compiledResource.data;

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
					textureHeader.format	= ImAppResPakTextureFormat_RGBA8;
					textureHeader.flags		= data.image.repeat ? ImAppResPakTextureFlags_Repeat : 0u;
					textureHeader.width		= data.image.width;
					textureHeader.height	= data.image.height;

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
							pushOutput( ResourceErrorLevel::Error, data.name, "Could not find '%s' in atlas.", data.name.getData() );
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
						pushOutput( ResourceErrorLevel::Error, data.name, "Could not find image '%s' for skin '%s'.", data.skin.imageName.getData(), data.name.getData() );
						continue;
					}

					const CompiledResource& imageResource = compiledResources[ imageIndex ];
					if( imageResource.data->image.allowAtlas )
					{
						const AtlasImage* atlasImage = m_atlasImages.find( imageResource.data->name );
						if( !atlasImage )
						{
							pushOutput( ResourceErrorLevel::Error, data.name, "Could not find '%s' in atlas.", imageResource.data->name.getData() );
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
							pushOutput( ResourceErrorLevel::Error, data.name, "Could not find texture '%s' for skin '%s'.", data.skin.imageName.getData(), data.name.getData() );
							continue;
						}

						skinHeader.x		= 0u;
						skinHeader.y		= 0u;
						skinHeader.width	= (uint16)imageResource.data->image.width;
						skinHeader.height	= (uint16)imageResource.data->image.height;
					}

					headerOffset = writeToBuffer( skinHeader );
					headerSize = sizeof( skinHeader );
				}
				break;

			case ImAppResPakType_Font:
				{
					// TODO: codepoints
					ImAppResPakFontHeader fontHeader;
					fontHeader.codepointCount	= 0u;
					fontHeader.ttfDataSize		= (uint32)data.fileData.getSizeInBytes();
					fontHeader.fontSize			= data.font.size;
					fontHeader.isScalable		= data.font.isScalable;
				}
				break;

			case ImAppResPakType_Theme:
				{
					ImAppResPakThemeHeader themeHeader;

					const ResourceTheme& theme = *compiledResource.data->theme.theme;
					const UiToolboxConfig& config = theme.getConfig();

					HashSet< uint16 > referencesSet;
					if( !findResourceIndex( themeHeader.fontIndex, resourceIndexMapping, ImAppResPakType_Font, theme.getFontName(), data.name ) )
					{
						pushOutput( ResourceErrorLevel::Error, data.name, "Could not find font '%s' in theme '%s'.", theme.getFontName().getData(), data.name.getData() );
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
							pushOutput( ResourceErrorLevel::Error, data.name, "Could not find skin '%s' for slot %d in theme '%s'.", skinName.getData(), i, data.name.getData() );
						}
						referencesSet.insert( themeHeader.skinIndices[ i ] );
					}

					for( uintsize i = 0u; i < TIKI_ARRAY_COUNT( themeHeader.imageIndices ); ++i )
					{
						const StringView imageName = theme.getImageName( (ImUiToolboxImage)i );
						if( !findResourceIndex( themeHeader.imageIndices[ i ], resourceIndexMapping, ImAppResPakType_Image, imageName, data.name ) )
						{
							pushOutput( ResourceErrorLevel::Error, data.name, "Could not find image '%s' for slot %d in theme '%s'.", imageName.getData(), i, data.name.getData() );
						}
						referencesSet.insert( themeHeader.imageIndices[ i ] );
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

	void ResourceCompiler::writeResourceData( uint32 resourcesOffset, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping )
	{
		for( uintsize compiledResourceIndex = 0u; compiledResourceIndex < compiledResources.getLength(); ++compiledResourceIndex )
		{
			const CompiledResource& compiledResource = compiledResources[ compiledResourceIndex ];
			const ResourceData& data = *compiledResource.data;

			uint32 dataOffset = 0u;
			uint32 dataSize = 0u;
			switch( compiledResource.type )
			{
			case ImAppResPakType_Texture:
				dataOffset	= writeArrayToBuffer< byte >( data.image.imageData );
				dataSize	= (uint32)data.image.imageData.getSizeInBytes();
				break;

			case ImAppResPakType_Image:
				break;

			case ImAppResPakType_Skin:
				break;

			case ImAppResPakType_Font:
				// TODO
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
	T& ResourceCompiler::getBufferData( uint32 offset )
	{
		return *(T*)&m_buffer[ offset ];
	}

	template< typename T >
	T& ResourceCompiler::getBufferArrayElement( uint32 offset, uintsize index )
	{
		return *(T*)&m_buffer[ offset + (sizeof( T ) * index) ];
	}

	template< typename T >
	uint32 ResourceCompiler::preallocateToBuffer( uintsize alignment /*= 1u */ )
	{
		return preallocateArrayToBuffer< T >( alignment );
	}

	template< typename T >
	uint32 ResourceCompiler::preallocateArrayToBuffer( uintsize length, uintsize alignment /* = 1u */ )
	{
		const uintsize alignedLength = alignValue( m_buffer.getLength(), alignment );
		m_buffer.setLengthZero( alignedLength );

		m_buffer.pushRange( sizeof( T ) * length );
		return (uint32)alignedLength;
	}

	template< typename T >
	uint32 ResourceCompiler::writeToBuffer( const T& value, uintsize alignment /* = 1u */ )
	{
		const uint32 offset = preallocateToBuffer< T >( alignment );
		getBufferData< T >( offset ) = value;
		return offset;
	}

	template< typename T >
	uint32 ResourceCompiler::writeArrayToBuffer( const ArrayView< T >& array, uintsize alignment /* = 1u */ )
	{
		const uintsize alignedLength = alignValue( m_buffer.getLength(), alignment );
		m_buffer.setLengthZero( alignedLength );

		m_buffer.pushRange( (const byte*)array.getData(), array.getSizeInBytes() );
		return (uint32)alignedLength;
	}

	bool ResourceCompiler::findResourceIndex( uint16& target, const ResourceTypeIndexMap& mapping, ImAppResPakType type, const DynamicString& name, const StringView& resourceName )
	{
		if( name.isEmpty() )
		{
			target = IMAPP_RES_PAK_INVALID_INDEX;
			return true;
		}

		const uint16* resourceIndex = mapping[ type ].find( name );
		if( !resourceIndex )
		{
			return false;
		}

		target = *resourceIndex;
		return true;
	}

	void ResourceCompiler::pushOutput( ResourceErrorLevel level, const StringView& resourceName, const char* format, ... )
	{
		ResourceCompilerOutput& output = m_outputs.pushBack();
		output.level		= ResourceErrorLevel::Error;
		output.resourceName	= resourceName;

		va_list args;
		va_start( args, format );
		output.message		= DynamicString::formatArgs( format, args );
		va_end( args );

		if( level >= ResourceErrorLevel::Error )
		{
			m_result = false;
		}
	}

}

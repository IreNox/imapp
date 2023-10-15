#include "resource_compiler.h"

#include "resource.h"
#include "resource_package.h"

#include "thread.h"

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

	bool ResourceCompiler::startCompile( const StringView& outputPath, const ResourcePackage& package )
	{
		if( isRunning() )
		{
			return false;
		}

		m_outputPath = outputPath;

		//HashSet< DynamicString > removedResources;
		//for( ResourceMap::ConstIterator it : m_resources )
		//{
		//	removedResources.
		//}

		for( const Resource& resource : package.getResources() )
		{
			if( resource.getType() != ResourceType::Image &&
				resource.getType() != ResourceType::Font &&
				resource.getType() != ResourceType::Skin &&
				resource.getType() != ResourceType::Theme )
			{
				continue;
			}

			ResourceData& data = m_resources[ resource.getName() ];

			data.type = resource.getType();
			data.name = resource.getName();

			if( (data.type == ResourceType::Image || data.type == ResourceType::Font) &&
				resource.getFileHash() != data.fileHash )
			{
				data.fileData = resource.getFileData();
				data.fileHash = resource.getFileHash();

				if( data.type == ResourceType::Image )
				{
					data.image.imageData = resource.getImageData();
				}
			}

			switch( resource.getType() )
			{
			case ResourceType::Image:
				data.image.width		= resource.getImageWidth();
				data.image.height		= resource.getImageHeight();
				data.image.allowAtlas	= resource.getImageAllowAtlas();
				data.image.isAtlas		= false;
				break;

			case ResourceType::Font:
				data.font.size			= resource.getFontSize();
				break;

			case ResourceType::Skin:
				data.skin.imageName		= resource.getSkinImageName();
				data.skin.border		= resource.getSkinBorder();
				break;

			case ResourceType::Theme:
				if( data.theme.theme )
				{
					*data.theme.theme = resource.getTheme();
				}
				else
				{
					data.theme.theme = new ResourceTheme( resource.getTheme() );
				}
				break;

			default:
				break;
			}

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

		ImAppResPakHeader& bufferHeader = preallocateToBuffer< ImAppResPakHeader >();
		bufferHeader.magic[ 0u ]	= 'I';
		bufferHeader.magic[ 1u ]	= 'A';
		bufferHeader.magic[ 2u ]	= 'R';
		bufferHeader.magic[ 3u ]	= '1';
		bufferHeader.resourceCount	= (uint16)compiledResources.getLength();

		Array< ImAppResPakResource > bufferResources = preallocateArrayToBuffer< ImAppResPakResource >( compiledResources.getLength() );

		for( uintsize i = 0u; i < resourcesByType.getLength(); ++i )
		{
			const DynamicArray< uint16 >& indices = resourcesByType[ i ];

			bufferHeader.resourcesByTypeIndexOffset[ i ]	= writeArrayToBuffer< uint16 >( indices );
			bufferHeader.resourcesbyTypeCount[ i ]			= (uint16)indices.getLength();
		}

		writeResourceNames( compiledResources );
		bufferHeader.resourcesOffset = (uint32)m_buffer.getLength();
		writeResourceData( bufferResources, compiledResources, resourceIndexMapping );

		FILE* file = fopen( m_outputPath.getData(), "wb" );
		if( !file )
		{
			pushOutput( ResourceErrorLevel::Error, "package", "Failed to open '%s'.", m_outputPath.getData() );
			return;
		}

		fwrite( m_buffer.getData(), m_buffer.getLength(), 1u, file );
		fclose( file );
	}

	bool ResourceCompiler::updateImageAtlas()
	{
		bool isAtlasUpToDate = true;
		DynamicArray< const ResourceData* > images;
		for( ResourceMap::Iterator it = m_resources.getBegin(); it != m_resources.getEnd(); ++it )
		{
			if( it->type != ResourceType::Image ||
				!it->image.allowAtlas ||
				it->image.imageData.isEmpty() )
			{
				continue;
			}

			AtlasImage* atlasImage = m_atlasImages.find( it.getKey() );
			if( atlasImage )
			{
				isAtlasUpToDate &= atlasImage->width == it->image.width;
				isAtlasUpToDate &= atlasImage->height == it->image.width;
			}
			else
			{
				isAtlasUpToDate = false;
			}

			images.pushBack( &*it );
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

		ByteArray imageData;
		for( const ResourceData* image : images )
		{
			const uintsize sourceLineSize		= image->image.width;
			const uintsize sourceImageSize		= sourceLineSize * image->image.height;
			const uintsize targetLineSize		= image->image.width + 2u;
			const uintsize targetImageSize		= targetLineSize * (image->image.height + 2u);

			imageData.clear();
			imageData.setLengthUninitialized( targetImageSize * 4u );

			// <= to compensate start at 1
			for( uintsize y = 1u; y <= image->image.height; ++y )
			{
				const uint32* sourceLineData	= (const uint32*)&image->image.imageData[ y * sourceLineSize ];
				uint32* targetLineData			= (uint32*)&imageData[ y * targetLineSize ];

				memcpy( targetLineData + 1u, sourceLineData, sourceLineSize );

				// fill first and list line border pixels
				targetLineData[ 0u ] = sourceLineData[ 0u ];
				targetLineData[ 1u + sourceLineSize ] = sourceLineData[ sourceLineSize - 1u ];
			}

			{
				const uint32* sourceData	= (const uint32*)image->image.imageData.getData();
				uint32* targetData			= (uint32*)imageData.getData();

				// fill border first and last line
				memcpy( targetData + 1u, sourceData, sourceLineSize );
				memcpy( (targetData + targetImageSize + 1u) - targetLineSize, (sourceData + sourceImageSize) - sourceLineSize, sourceLineSize );

				// fill border corner pixels
				targetData[ 0u ]								= sourceData[ 0u ];									// TL
				targetData[ targetLineSize - 1u ]				= sourceData[ sourceLineSize - 1u ];				// TR
				targetData[ targetImageSize - targetLineSize ]	= sourceData[ sourceImageSize - sourceLineSize ];	// BL
				targetData[ targetImageSize - 1u ]				= sourceData[ sourceImageSize - 1u ];				// BR
			}

			int x;
			int y;
			const kia_result imageResult = K15_IAAddImageToAtlas( &atlas, KIA_PIXEL_FORMAT_R8G8B8A8, imageData.getData(), image->image.width, image->image.height, &x, &y );
			if( imageResult != K15_IA_RESULT_SUCCESS )
			{
				pushOutput( ResourceErrorLevel::Error, image->name, "Failed to add '%s' to atlas. Error: %d", image->name.getData(), imageResult );
				m_atlasImages.clear();
				return false;
			}

			AtlasImage& atlasImage = m_atlasImages[ image->name ];
			atlasImage.x		= (uint16)x;
			atlasImage.y		= (uint16)y;
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
		m_atlasData.image.isAtlas		= true;

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

		for( const ResourceData& resource : m_resources )
		{
			uint16 refIndex = 0u; // 0 == texture or unused

			if( resource.type == ResourceType::Image &&
				resource.image.imageData.isEmpty() )
			{
				pushOutput( ResourceErrorLevel::Warning, resource.name, "Image '%s' has no pixel data.", resource.name.getData() );
				continue;
			}

			if( resource.type == ResourceType::Image &&
				!resource.image.allowAtlas )
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
		}
	}

	void ResourceCompiler::writeResourceData( Array< ImAppResPakResource >& targetResources, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping )
	{
		for( uintsize compiledResourceIndex = 0u; compiledResourceIndex < compiledResources.getLength(); ++compiledResourceIndex )
		{
			const CompiledResource& compiledResource = compiledResources[ compiledResourceIndex ];
			const ResourceData& data = *compiledResource.data;

			ImAppResPakResource& targetResource = targetResources[ compiledResourceIndex ];

			targetResource.type			= compiledResource.type;
			targetResource.offset		= 0u;
			targetResource.size			= 0u;
			targetResource.nameOffset	= compiledResource.nameOffset;
			targetResource.nameLength	= compiledResource.nameLength;

			switch( compiledResource.type )
			{
			case ImAppResPakType_Texture:
				{
					ImAppResPakTextureData& textureData = preallocateToBufferOffset< ImAppResPakTextureData >( targetResource.offset );
					textureData.format	= ImAppResPakTextureFormat_RGBA8;
					textureData.width	= data.image.width;
					textureData.width	= data.image.height;

					writeArrayToBuffer< byte >( data.image.imageData );

					targetResource.size = uint32( sizeof( textureData ) + data.image.imageData.getLength() );
				}
				break;

			case ImAppResPakType_Image:
				{
					targetResource.textureIndex = compiledResource.refIndex;

					ImAppResPakImageData& imageData = preallocateToBufferOffset< ImAppResPakImageData >( targetResource.offset );
					if( data.image.allowAtlas )
					{
						const AtlasImage* atlasImage = m_atlasImages.find( data.name );
						if( !atlasImage )
						{
							pushOutput( ResourceErrorLevel::Error, data.name, "Could not find '%s' in atlas.", data.name.getData() );
							continue;
						}

						imageData.x			= atlasImage->x;
						imageData.y			= atlasImage->y;
						imageData.width		= atlasImage->width;
						imageData.height	= atlasImage->height;
					}
					else
					{
						imageData.x			= 0u;
						imageData.y			= 0u;
						imageData.width		= (uint16)data.image.width;
						imageData.height	= (uint16)data.image.height;
					}

					targetResource.size = sizeof( imageData );
				}
				break;

			case ImAppResPakType_Skin:
				{
					ImAppResPakSkinData& skinData = preallocateToBufferOffset< ImAppResPakSkinData >( targetResource.offset );

					skinData.top	= data.skin.border.top;
					skinData.left	= data.skin.border.left;
					skinData.right	= data.skin.border.right;
					skinData.bottom	= data.skin.border.bottom;

					uint16 imageIndex;
					if( !findResourceIndex( imageIndex, resourceIndexMapping, ImAppResPakType_Image, data.skin.imageName, data.name ) )
					{
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

						skinData.x		= atlasImage->x;
						skinData.y		= atlasImage->y;
						skinData.width	= atlasImage->width;
						skinData.height	= atlasImage->height;
					}
					else
					{
						skinData.x		= 0u;
						skinData.y		= 0u;
						skinData.width	= (uint16)imageResource.data->image.width;
						skinData.height	= (uint16)imageResource.data->image.height;
					}

					targetResource.size = sizeof( skinData );
				}
				break;

			case ImAppResPakType_Font:
				break;

			case ImAppResPakType_Theme:
				{
					ImAppResPakThemeData& themeData = preallocateToBufferOffset< ImAppResPakThemeData >( targetResource.offset );

					const ResourceTheme& theme = *compiledResource.data->theme.theme;
					const UiToolboxConfig& config = theme.getConfig();

					findResourceIndex( themeData.fontIndex, resourceIndexMapping, ImAppResPakType_Font, theme.getFontName(), data.name );

					for( uintsize i = 0u; i < TIKI_ARRAY_COUNT( themeData.colors ); ++i )
					{
						themeData.colors[ i ] = config.colors[ i ];
					}

					for( uintsize i = 0u; i < TIKI_ARRAY_COUNT( themeData.skinIndices ); ++i )
					{
						findResourceIndex( themeData.skinIndices[ i ], resourceIndexMapping, ImAppResPakType_Skin, theme.getSkinName( (ImUiToolboxSkin)i ), data.name );
					}

					for( uintsize i = 0u; i < TIKI_ARRAY_COUNT( themeData.imageIndices ); ++i )
					{
						findResourceIndex( themeData.imageIndices[ i ], resourceIndexMapping, ImAppResPakType_Image, theme.getImageName( (ImUiToolboxImage)i ), data.name );
					}

					themeData.button		= config.button;
					themeData.checkBox		= config.checkBox;
					themeData.slider		= config.slider;
					themeData.textEdit		= config.textEdit;
					themeData.progressBar	= config.progressBar;
					themeData.scrollArea	= config.scrollArea;
					themeData.list			= config.list;
					themeData.dropDown		= config.dropDown;
					themeData.popup			= config.popup;

					targetResource.size = sizeof( themeData );
				}
				break;

			case ImAppResPakType_MAX:
				break;
			}
		}
	}

	template< typename T >
	T& ResourceCompiler::preallocateToBuffer( uintsize alignment /*= 1u */ )
	{
		uint32 offset;
		return preallocateToBufferOffset< T >( offset );
	}

	template< typename T >
	T& ResourceCompiler::preallocateToBufferOffset( uint32& offset, uintsize alignment /* = 1u */ )
	{
		return preallocateArrayToBufferOffset< T >( 1u, offset, alignment ).getFront();
	}

	template< typename T >
	Array< T > ResourceCompiler::preallocateArrayToBuffer( uintsize length, uintsize alignment /* = 1u */ )
	{
		uint32 offset;
		return preallocateArrayToBufferOffset< T >( length, offset, alignment );
	}

	template< typename T >
	Array< T > ResourceCompiler::preallocateArrayToBufferOffset( uintsize length, uint32& offset, uintsize alignment /* = 1u */ )
	{
		const uintsize alignedLength = alignValue( m_buffer.getLength(), alignment );
		m_buffer.setLengthZero( alignedLength );

		offset = (uint32)alignedLength;

		Array< byte > data = m_buffer.pushRange( sizeof( T ) * length );
		return Array< T >( (T*)data.getData(), length );
	}

	template< typename T >
	uint32 ResourceCompiler::writeToBuffer( const T& value, uintsize alignment /* = 1u */ )
	{
		uint32 offset;
		preallocateToBufferOffset< T >( offset, alignment ) = value;
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
			pushOutput( ResourceErrorLevel::Error, resourceName, "Could not find resource '%s'.", name.getData() );
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

#include "resource_compiler.h"

#include "resource.h"
#include "resource_package.h"

#include "thread.h"

#include <imapp/../../src/imapp_res_pak.h>

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

	ResourceCompiler::ResourceCompiler( ResourcePackage& package )
		: m_package( package )
	{
	}

	ResourceCompiler::~ResourceCompiler()
	{
		if( m_thread )
		{
			destroyThread( m_thread );
		}
	}

	bool ResourceCompiler::startCompile( const StringView& outputPath )
	{
		if( isRunning() )
		{
			return false;
		}

		//HashSet< DynamicString > removedResources;
		//for( ResourceMap::ConstIterator it : m_resources )
		//{
		//	removedResources.
		//}

		for( const Resource& resource : m_package.getResources() )
		{
			if( resource.getType() != ResourceType::Image &&
				resource.getType() != ResourceType::Font  &&
				resource.getType() != ResourceType::Theme )
			{
				continue;
			}

			ResourceData& data = m_resources[ resource.getName() ];

			data.type = resource.getType();

			if( (resource.getType() == ResourceType::Image || resource.getType() == ResourceType::Font) &&
				resource.getFileHash() != data.dataHash )
			{
				data.data = resource.getFileData();
				data.dataHash = resource.getFileHash();
			}

			switch( resource.getType() )
			{
			case ResourceType::Image:
				data.image.width		= resource.getImageWidth();
				data.image.height		= resource.getImageHeight();
				data.image.allowAtlas	= resource.getImageAllowAtlas();
				break;

			case ResourceType::Font:
				data.font.size			= resource.getFontSize();
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

	bool ResourceCompiler::isRunning() const
	{
		if( !m_thread )
		{
			return false;
		}

		return isThreadRunning( m_thread );
	}

	/*static*/ void ResourceCompiler::staticRunCompileThread( void* arg )
	{
		ResourceCompiler* compiler = (ResourceCompiler*)arg;
		compiler->runCompileThread();
	}

	void ResourceCompiler::runCompileThread()
	{
		m_buffer.clear();

		if( !updateImageAtlas() )
		{
			return;
		}

		ImAppResPakHeader header;
		header.magic[ 0u ]		= 'I';
		header.magic[ 1u ]		= 'A';
		header.magic[ 2u ]		= 'R';
		header.magic[ 3u ]		= '1';
		header.resourceCount	= (uint16)m_package.getResourceCount();

		for( uintsize i = 0; i < m_package.getResourceCount(); ++i )
		{
			const Resource& resource = m_package.getResource( i );
		}

	}

	bool ResourceCompiler::updateImageAtlas()
	{
		bool isAtlasUpToDate = true;
		DynamicArray< const ResourceData* > images;
		for( ResourceMap::Iterator it = m_resources.getBegin(); it != m_resources.getEnd(); ++it )
		{
			if( it->type != ResourceType::Image ||
				!it->image.allowAtlas )
			{
				continue;
			}

			images.pushBack( &*it );

			AtlasImage* atlasImage = m_atlasImages.find( it.getKey() );
			if( !atlasImage )
			{
				isAtlasUpToDate = false;
				break;
			}

			isAtlasUpToDate &= atlasImage->width == it->image.width;
			isAtlasUpToDate &= atlasImage->height == it->image.width;
		}

		if( isAtlasUpToDate )
		{
			return true;
		}

		K15_ImageAtlas atlas;
		if( K15_IACreateAtlas( &atlas, (kia_u32)images.getLength() ) != K15_IA_RESULT_SUCCESS )
		{
			return false;
		}

		return true;
	}

}
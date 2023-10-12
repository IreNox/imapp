#include "resource_compiler.h"

#include "resource_package.h"

#include <imapp/../../src/imapp_res_pak.h>

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
	}

	bool ResourceCompiler::compile( const StringView& outputPath )
	{
		m_buffer.clear();

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

		return true;
	}

}
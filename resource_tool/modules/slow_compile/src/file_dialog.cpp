#include "file_dialog.h"

#include <portable-file-dialogs.h>

namespace imapp
{
	DynamicString openFileDialog( const StringView& title, const StringView& initialPath, const ConstArrayView< StringView >& filters )
	{
		std::vector< std::string > stdFilters;
		stdFilters.reserve( filters.getLength() );
		for( const StringView& filter : filters )
		{
			stdFilters.push_back( filter.toConstCharPointer() );
		}

		const std::vector< std::string > results = pfd::open_file( title.toConstCharPointer(), initialPath.toConstCharPointer(), stdFilters, pfd::opt::none ).result();
		if( results.empty() )
		{
			return DynamicString();
		}

		return DynamicString( results[ 0u ].data(), results[ 0u ].size());
	}

	DynamicString saveFileDialog( const StringView& title, const StringView& initialPath, const ConstArrayView< StringView >& filters )
	{
		std::vector< std::string > stdFilters;
		stdFilters.reserve( filters.getLength() );
		for( const StringView& filter : filters )
		{
			stdFilters.push_back( filter.toConstCharPointer() );
		}

		const std::string result = pfd::save_file( title.toConstCharPointer(), initialPath.toConstCharPointer(), stdFilters, pfd::opt::none ).result();
		return DynamicString( result.data(), result.size() );
	}
}

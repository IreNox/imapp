#include "imapp_res_pak.h"

#include "imapp_types.h"

const uint16_t* ImAppResPakResourcesByType( const void* base, ImAppResPakType type )
{
	const ImAppResPakHeader* header	= (const ImAppResPakHeader*)base;
	const uint32 offset				= header->resourcesByTypeIndexOffset[ type ];

	const byte* bytes = (const byte*)base;
	bytes += offset;

	return (const uint16*)bytes;
}

uint16_t ImAppResPakResourcesByTypeCount( const void* base, ImAppResPakType type )
{
	const ImAppResPakHeader* header	= (const ImAppResPakHeader*)base;
	return header->resourcesbyTypeCount[ type ];
}

const ImAppResPakResource* ImAppResPakResourceGet( const void* base, uint16_t index )
{
	const ImAppResPakHeader* header	= (const ImAppResPakHeader*)base;
	if( index >= header->resourceCount )
	{
		return NULL;
	}

	const byte* bytes = (const byte*)base;
	bytes += sizeof( ImAppResPakHeader );
	bytes += sizeof( ImAppResPakResource ) * index;

	return (const ImAppResPakResource*)bytes;
}

//ImAppResPakResource* ImAppResPakResourceFind( const void* base, const char* name, ImAppResPakType type )
//{
//}

ImUiStringView ImAppResPakResourceGetName( const void* base, const ImAppResPakResource* res )
{
	const byte* bytes = (const byte*)base;
	bytes += res->nameOffset;

	return ImUiStringViewCreateLength( (const char*)bytes, res->nameLength );
}

const void* ImAppResPakResourceGetHeader( const void* base, const ImAppResPakResource* res )
{
	const byte* bytes = (const byte*)base;
	bytes += res->headerOffset;

	return bytes;
}

const void* ImAppResPakResourceGetData( const void* base, const ImAppResPakResource* res )
{
	const byte* bytes = (const byte*)base;
	bytes += res->dataOffset;

	return bytes;
}

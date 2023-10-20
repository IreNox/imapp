#include "resource_helpers.h"

#include <tiki/tiki_string_tools.h>

#include <tinyxml2.h>

namespace imapp
{
	XMLElement* findOrCreateElement( XMLNode* node, const char* name )
	{
		XMLElement* child = node->FirstChildElement( name );
		if( !child )
		{
			child = node->GetDocument()->NewElement( name );
			node->InsertEndChild( child );
		}

		return child;
	}

	bool parseUiColor( ImUiColor& target, const StringView& string )
	{
		if( string.getLength() >= 6u )
		{
			ImUiColor result;

			bool ok = true;
			ok &= string_tools::tryParseHexUInt8( result.red, string.subString( 0u, 2u ) );
			ok &= string_tools::tryParseHexUInt8( result.green, string.subString( 2u, 2u ) );
			ok &= string_tools::tryParseHexUInt8( result.blue, string.subString( 4u, 2u ) );

			if( string.getLength() == 4u )
			{
				ok &= string_tools::tryParseHexUInt8( result.alpha, string.subString( 6u, 2u ) );
			}
			else
			{
				result.alpha = 0xffu;
			}

			if( !ok )
			{
				return false;
			}

			target = result;
			return true;
		}
		else if( string.getLength() >= 3u )
		{
			ImUiColor result;

			bool ok = true;
			ok &= string_tools::tryParseHexNibble( result.red, string.subString( 0u, 1u ) );
			ok &= string_tools::tryParseHexNibble( result.green, string.subString( 1u, 1u ) );
			ok &= string_tools::tryParseHexNibble( result.blue, string.subString( 2u, 1u ) );

			if( string.getLength() == 4u )
			{
				ok &= string_tools::tryParseHexNibble( result.alpha, string.subString( 3u, 1u ) );
			}
			else
			{
				result.alpha = 0x0fu;
			}

			if( !ok )
			{
				return false;
			}

			result.red			|= result.red << 4u;
			result.green		|= result.green << 4u;
			result.blue			|= result.blue << 4u;
			result.alpha		|= result.alpha << 4u;

			target = result;
			return true;
		}

		return false;
	}

	uintsize formatUiColor( char* buffer, uintsize bufferSize, const ImUiColor& value )
	{
		bool isNibbleUniform = true;
		const byte* bytes = &value.red;
		byte uniformBytes[ 4u ];
		for( uintsize i = 0u; i < 4u; ++i )
		{
			const uint8 b = bytes[ i ];
			isNibbleUniform &= (b & 0x0fu) == (b >> 4u);
			uniformBytes[ i ] = b & 0x0fu;
		}

		char format[] = "%02x%02x%02x%02x";
		if( value.alpha == 0xffu )
		{
			format[ 12u ] = '\0';
		}
		if( isNibbleUniform )
		{
			format[ 2u ]	= '1';
			format[ 6u ]	= '1';
			format[ 10u ]	= '1';
			format[ 14u ]	= '1';

			bytes = uniformBytes;
		}

		return (uintsize)snprintf( buffer, bufferSize, format, bytes[ 0u ], bytes[ 1u ], bytes[ 2u ], bytes[ 3u ] );
	}
}

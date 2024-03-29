#pragma once

#include <imui/imui_cpp.h>

#include <tiki/tiki_dynamic_string.h>

namespace tinyxml2
{
	class XMLElement;
	class XMLNode;
}

namespace imapp
{
	using namespace imui;
	using namespace tiki;
	using namespace tinyxml2;

	//class RtStr : public StringView
	//{
	//public:

	//	RtStr( const char* str )
	//		: StringView( str )
	//	{
	//	}

	//	RtStr( const StringView& str )
	//		: StringView( str )
	//	{
	//	}

	//	RtStr( const UiStringView& str )
	//		: StringView( str.data, str.length )
	//	{
	//	}

	//	RtStr( const DynamicString& str )
	//		: StringView( str.getBegin(), str.getLength() )
	//	{
	//	}

	//	operator UiStringView()
	//	{
	//		return UiStringView( m_data, m_length );
	//	}
	//};

	XMLElement*		findOrCreateElement( XMLNode* node, const char* name );

	bool			parseUiColor( ImUiColor& target, const StringView& string );
	uintsize		formatUiColor( char* buffer, uintsize bufferSize, const ImUiColor& value );
}

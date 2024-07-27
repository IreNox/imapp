#pragma once

#include "resource_types.h"

#include <tiki/tiki_dynamic_string.h>

#include <imui/imui_cpp.h>

namespace imapp
{
	using namespace imui;
	using namespace tiki;

	class CompilerOutput;
	class Resource;
	class ResourceTheme;

	struct CompilerFontData
	{
		using ByteArray = DynamicArray< byte >;
		using CodepointArray = DynamicArray< ImUiFontCodepoint >;

		ByteArray					pixelData;
		uint32						width;
		uint32						height;

		CodepointArray				codepoints;
	};

	class CompilerResourceData
	{
	public:

		using ByteArray = DynamicArray< byte >;
		using ByteView = ConstArrayView< byte >;
		using FontUnicodeBlockArray = DynamicArray< ResourceFontUnicodeBlock >;

		struct ImageData
		{
			uint32					width			= 0u;
			uint32					height			= 0u;
			bool					allowAtlas		= false;
			bool					repeat			= false;
			ByteArray				imageData;
		};

		struct FontData
		{
			FontUnicodeBlockArray	blocks;

			float					size			= 0u;
			bool					isScalable		= false;
		};

		struct SkinData
		{
			DynamicString			imageName;
			UiBorder				border;
		};

		struct ThemeData
		{
			ResourceTheme*			theme			= nullptr;
		};

		struct ResourceData
		{
			ResourceType			type			= ResourceType::Count;
			DynamicString			name;
			DynamicString			filePath;
			ImUiHash				fileHash		= 0u;
			ByteArray				fileData;

			ImageData				image;
			FontData				font;
			SkinData				skin;
			ThemeData				theme;
		};

									CompilerResourceData();
									CompilerResourceData( const Resource& resource );
									~CompilerResourceData();

		const ResourceData&			getData() const { return m_data; }
		bool						isAtlasImage() const;

		void						applyResource( const Resource& resource );
		void						applyResourceData( const ResourceData& resourceData );

		ByteView					compileImageAtlasData();
		const CompilerFontData*		compileFont( CompilerOutput& output );
		const CompilerFontData*		compileFontSdf( CompilerOutput& output );

	private:

		ResourceData				m_data;

		ByteArray					m_imageAtlasData;
		uint32						m_imageAtlasRevision	= 0u;

		CompilerFontData			m_fontData;
		uint32						m_fontRevision			= 0u;

		CompilerFontData			m_fontSdfData;
		uint32						m_fontSdfRevision		= 0u;

		uint32						m_revision				= 0u;

		void						convertSdfBitmapLine( ArrayView< uint32 > targetLine, const float* sourceLine, uintsize charSize );
		uint32						convertSdfBitmapPixel( const float* source );
	};
}
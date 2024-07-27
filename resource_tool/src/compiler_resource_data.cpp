#include "compiler_resource_data.h"

#include "compiler_output.h"
#include "resource.h"

#include <msdfgen.h>
#include <ext/import-font.h>
#include <ext/save-png.h>

#include <cmath>

namespace imapp
{
	CompilerResourceData::CompilerResourceData()
	{
	}

	CompilerResourceData::CompilerResourceData( const Resource& resource )
	{
		applyResource( resource );
	}

	CompilerResourceData::~CompilerResourceData()
	{
	}

	bool CompilerResourceData::isAtlasImage() const
	{
		return m_data.type == ResourceType::Image &&
			m_data.image.allowAtlas &&
			m_data.image.imageData.hasElements();
	}

	void CompilerResourceData::applyResource( const Resource& resource )
	{
		if( m_revision == resource.getRevision() )
		{
			return;
		}

		m_data.type		= resource.getType();
		m_data.name		= resource.getName();
		m_data.filePath	= resource.getFileSourcePath();

		if( (m_data.type == ResourceType::Image || m_data.type == ResourceType::Font) &&
			resource.getFileHash() != m_data.fileHash )
		{
			m_data.fileData = resource.getFileData();
			m_data.fileHash = resource.getFileHash();

			if( m_data.type == ResourceType::Image )
			{
				m_data.image.imageData = resource.getImageData();
			}
		}

		switch( resource.getType() )
		{
		case ResourceType::Image:
			m_data.image.width		= resource.getImageWidth();
			m_data.image.height		= resource.getImageHeight();
			m_data.image.allowAtlas	= resource.getImageAllowAtlas();
			m_data.image.repeat		= resource.getImageRepeat();
			break;

		case ResourceType::Font:
			m_data.font.blocks.assign( resource.getFontUnicodeBlocksView() );

			m_data.font.size		= resource.getFontSize();
			m_data.font.isScalable	= resource.getFontIsScalable();
			break;

		case ResourceType::Skin:
			m_data.skin.imageName	= resource.getSkinImageName();
			m_data.skin.border		= resource.getSkinBorder();
			break;

		case ResourceType::Theme:
			if( m_data.theme.theme )
			{
				*m_data.theme.theme = resource.getTheme();
			}
			else
			{
				m_data.theme.theme = new ResourceTheme( resource.getTheme() );
			}
			break;

		default:
			break;
		}

		m_revision = resource.getRevision();
	}

	void CompilerResourceData::applyResourceData( const ResourceData& resourceData )
	{
		m_data = resourceData;
		m_revision++;
	}

	ConstArrayView< byte > CompilerResourceData::compileImageAtlasData()
	{
		if( m_imageAtlasRevision == m_revision )
		{
			return m_imageAtlasData;
		}

		const ArrayView< uint32 > sourceData = m_data.image.imageData.cast< uint32 >();

		const uintsize sourceLineSize		= m_data.image.width;
		const uintsize sourceImageSize		= sourceLineSize * m_data.image.height;
		const uintsize targetLineSize		= m_data.image.width + 2u;
		const uintsize targetImageSize		= targetLineSize * (m_data.image.height + 2u);

		m_imageAtlasData.clear();
		m_imageAtlasData.setLengthUninitialized( targetImageSize * 4u );
		ArrayView< uint32 > targetData = m_imageAtlasData.cast< uint32 >();

		// <= to compensate start at 1
		for( uintsize y = 1u; y <= m_data.image.height; ++y )
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

		m_imageAtlasRevision = m_revision;
		return m_imageAtlasData;
	}

	const CompilerFontData* CompilerResourceData::compileFont( CompilerOutput& output )
	{
		if( m_fontRevision == m_revision )
		{
			return &m_fontData;
		}

		ImUiParameters imuiParams = {};
		ImUiContext* imui = ImUiCreate( &imuiParams );
		if( !imui )
		{
			output.pushMessage( CompilerErrorLevel::Error, m_data.name, "Failed to create IMUI." );
			return nullptr;
		}

		ImUiFontTrueTypeData* ttf = ImUiFontTrueTypeDataCreate( imui, m_data.fileData.getData(), m_data.fileData.getLength() );
		if( !ttf )
		{
			output.pushMessage( CompilerErrorLevel::Error, m_data.name, "Failed to load font file. Path: %s", m_data.filePath.toConstCharPointer() );
			ImUiDestroy( imui );
			return nullptr;
		}

		for( const ResourceFontUnicodeBlock& block : m_data.font.blocks )
		{
			ImUiFontTrueTypeDataAddCodepointRange( ttf, block.first, block.last );
		}

		ImUiFontTrueTypeDataCalculateMinTextureSize( ttf, m_data.font.size, &m_fontData.width, &m_fontData.height );
		m_fontData.width = (m_fontData.width + 4u - 1u) & (0 - 4);
		m_fontData.height = (m_fontData.height + 4u - 1u) & (0 - 4);

		m_fontData.pixelData.setLengthUninitialized( m_fontData.width * m_fontData.height );

		ImUiFontTrueTypeImage* ttfImage = ImUiFontTrueTypeDataGenerateTextureData( ttf, m_data.font.size, m_fontData.pixelData.getData(), m_fontData.pixelData.getLength(), m_fontData.width, m_fontData.height );
		if( !ttfImage )
		{
			output.pushMessage( CompilerErrorLevel::Error, m_data.name, "Failed to generate font pixel data. Path: %s", m_data.filePath.toConstCharPointer() );
			ImUiFontTrueTypeDataDestroy( ttf );
			ImUiDestroy( imui );
			return nullptr;
		}

		{
			const ImUiFontCodepoint* codepoints = nullptr;
			size_t codepointCount = 0u;
			ImUiFontTrueTypeImageGetCodepoints( ttfImage, &codepoints, &codepointCount );

			m_fontData.codepoints.assign( codepoints, codepointCount );
		}

		ImUiFontTrueTypeImageDestroy( ttfImage );
		ImUiFontTrueTypeDataDestroy( ttf );
		ImUiDestroy( imui );

		m_fontRevision = m_revision;
		return &m_fontData;
	}

	const CompilerFontData* CompilerResourceData::compileFontSdf( CompilerOutput& output )
	{
		using namespace msdfgen;

		static constexpr uintsize CharSize = 32u;

		//if( m_fontSdfRevision == m_revision )
		//{
		//	return &m_fontSdfData;
		//}

		FreetypeHandle* ft = initializeFreetype();
		if( !ft )
		{
			output.pushMessage( CompilerErrorLevel::Error, m_data.name, "Failed to initialize FreeType" );
			deinitializeFreetype( ft );
			return nullptr;
		}

		FontHandle* font = loadFontData( ft, m_data.fileData.getData(), (int)m_data.fileData.getLength() );
		if( !font )
		{
			output.pushMessage( CompilerErrorLevel::Error, m_data.name, "Failed to load font. Path: %s", m_data.filePath.toConstCharPointer() );
			deinitializeFreetype( ft );
			return nullptr;
		}

		uintsize codepointCount = 0u;
		for( const ResourceFontUnicodeBlock& block : m_data.font.blocks )
		{
			codepointCount += (block.last - block.first) + 1u;
		}

		const uintsize columnCount		= (uintsize)ceil( sqrt( (double)codepointCount ) );
		const uintsize rowCount			= (codepointCount + (columnCount - 1u)) / columnCount;
		const uintsize borderCharSize	= CharSize + 2u;
		const uintsize pixelDataWidth	= columnCount * borderCharSize;
		const uintsize pixelDataHeight	= rowCount * borderCharSize;
		const uintsize pixelDataSize	= pixelDataWidth * pixelDataHeight * 4u;

		m_fontSdfData.pixelData.setLengthUninitialized( pixelDataSize );
		m_fontSdfData.codepoints.reserve( codepointCount );
		m_fontSdfData.width		= (uint32)pixelDataWidth;
		m_fontSdfData.height	= (uint32)pixelDataHeight;

		ArrayView< uint32 > targetData = m_fontSdfData.pixelData.cast< uint32 >();

		double advance;
		Shape shape;
		uintsize codepointIndex = 0u;
		Bitmap< float, 3 > sdfBitmap( (int)CharSize, (int)CharSize );
		Projection projection; // (1.0, Vector2( 0.125, 0.125 ));
		for( const ResourceFontUnicodeBlock& block : m_data.font.blocks )
		{
			for( uint32 codepointValue = block.first; codepointValue <= block.last; ++codepointValue )
			{
				if( !loadGlyph( shape, font, codepointValue, &advance ) )
				{
					destroyFont( font );
					deinitializeFreetype( ft );
					return nullptr;
				}

				//shape.normalize();
				edgeColoringSimple( shape, 3.0 );

				generateMSDF( sdfBitmap, shape, projection, 0.125 );

				DynamicString path = DynamicString::format( "d:\\test\\sdf_%d.bmp", codepointValue );
				saveBmp( sdfBitmap, path.toConstCharPointer() );

				const uintsize columnIndex			= codepointIndex % columnCount;
				const uintsize rowIndex				= codepointIndex / columnCount;
				const uintsize targetBaseOffset		= rowIndex * borderCharSize * pixelDataWidth;
				const uintsize targetLineBaseOffset	= columnIndex * borderCharSize;

				for( uintsize y = 0u; y < CharSize; ++y )
				{
					const uintsize targetLineOffset = targetBaseOffset + ((y + 1u) * pixelDataWidth) + targetLineBaseOffset;

					const float* sourceLineData			= sdfBitmap( 0, int( (CharSize - 1u) - y ) );
					ArrayView< uint32 > targetLineData	= targetData.getRange( targetLineOffset, borderCharSize );

					convertSdfBitmapLine( targetLineData, sourceLineData, CharSize );
				}

				const uintsize targetFirstLineOffset = targetBaseOffset + targetLineBaseOffset;
				convertSdfBitmapLine( targetData.getRange( targetFirstLineOffset, borderCharSize ), sdfBitmap( 0, CharSize - 1 ), CharSize );

				const uintsize targetLastLineOffset = targetBaseOffset + (CharSize * pixelDataWidth) + targetLineBaseOffset;
				convertSdfBitmapLine( targetData.getRange( targetLastLineOffset, borderCharSize ), sdfBitmap( 0, 0 ), CharSize );

				Scanline scanline;
				shape.scanline( scanline, 0.0 );

				ImUiFontCodepoint& codepoint = m_fontSdfData.codepoints.pushBack();
				codepoint.codepoint		= codepointValue;
				codepoint.advance		= (float)advance;
				codepoint.width			= CharSize;
				codepoint.height		= CharSize;
				codepoint.ascentOffset	= 0.0f;
				codepoint.uv.u0			= float( targetLineBaseOffset + 1u ) / pixelDataWidth;
				codepoint.uv.v0			= float( (rowIndex * borderCharSize) + 1u) / pixelDataWidth;
				codepoint.uv.u1			= codepoint.uv.u0 + (CharSize / pixelDataWidth);
				codepoint.uv.v1			= codepoint.uv.v0 + (CharSize / pixelDataWidth);

				codepointIndex++;
			}
		}

		FILE* f = fopen( "d:\\test\\raw.rgb", "wb" );
		if( f )
		{
			fwrite( m_fontSdfData.pixelData.getData(), 1u, m_fontSdfData.pixelData.getLength(), f );
			fclose( f );
		}

		destroyFont( font );
		deinitializeFreetype( ft );

		//m_fontSdfRevision = m_revision;
		return &m_fontSdfData;
	}

	void CompilerResourceData::convertSdfBitmapLine( ArrayView< uint32 > targetLine, const float* sourceLine, uintsize charSize )
	{
		targetLine[ 0u ]			= convertSdfBitmapPixel( &sourceLine[ 0u ] );
		targetLine[ 1u + charSize ]	= convertSdfBitmapPixel( &sourceLine[ (charSize - 1u) * 3u ] );

		for( uintsize x = 0u; x < charSize; ++x )
		{
			targetLine[ x + 1u ] = convertSdfBitmapPixel( &sourceLine[ x * 3u ] );
		}
	}

	uint32 CompilerResourceData::convertSdfBitmapPixel( const float* source )
	{
		const uint32 r = uint32( clamp( (source[ 0u ] * 1.0f) + 127.0f, 0.0f, 255.0f ) );
		const uint32 g = uint32( clamp( (source[ 1u ] * 1.0f) + 127.0f, 0.0f, 255.0f ) );
		const uint32 b = uint32( clamp( (source[ 2u ] * 1.0f) + 127.0f, 0.0f, 255.0f ) );
		return (r << 24u) | (g << 16u) | (b << 8u) | 0xffu;
	}
}

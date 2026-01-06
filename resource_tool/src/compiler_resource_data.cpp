#include "compiler_resource_data.h"

#include "compiler_output.h"
#include "resource.h"

#if TIKI_ENABLED( IMAPP_RESOURCE_TOOL_MSDF )
#	include <msdf-atlas-gen.h>
#	include <glyph-generators.h>
#endif

// debug
#include <spng/spng.h>

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
#if TIKI_DISABLED( IMAPP_RESOURCE_TOOL_MSDF )
		output.pushMessage( CompilerErrorLevel::Error, m_data.name, "SDF Font supported is disabled. Please compile the resource tool with `use_msdf=on`." );
		return nullptr;
#else
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

		std::vector< msdf_atlas::GlyphGeometry > glyphs;
		msdf_atlas::FontGeometry fontGeometry( &glyphs );
		for( const ResourceFontUnicodeBlock& block : m_data.font.blocks )
		{
			msdf_atlas::Charset charset;
			for( uint32 codepointValue = block.first; codepointValue <= block.last; ++codepointValue )
			{
				charset.add( codepointValue );
			}

			const int glyphsLoaded = fontGeometry.loadCharset( font, 1.0f, charset );
			if( glyphsLoaded != charset.size() )
			{
				output.pushMessage( CompilerErrorLevel::Warning, m_data.name, "Failed to load %d codepoints in block '%s'", charset.size() - glyphsLoaded, block.name.toConstCharPointer());
			}
		}

		msdf_atlas::TightAtlasPacker atlasPacker;
		//if( fixedDimensions )
		//	atlasPacker.setDimensions( fixedWidth, fixedHeight );
		//else
		atlasPacker.setDimensionsConstraint( msdf_atlas::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE );

		//if( fixedScale )
		//	atlasPacker.setScale( config.emSize );
		//else
		atlasPacker.setMinimumScale( m_data.font.size ); // 32.0 );

		atlasPacker.setPixelRange( msdfgen::Range( -1.0, 1.0 ) );
		//atlasPacker.setUnitRange( emRange );
		atlasPacker.setMiterLimit( 1.0 );
		atlasPacker.setOriginPixelAlignment( false, true );

		if( int remaining = atlasPacker.pack( glyphs.data(), (int)glyphs.size() ) )
		{
			if( remaining < 0 )
			{
				output.pushMessage( CompilerErrorLevel::Error, m_data.name, "Failed to pack glyphs into atlas." );
				destroyFont( font );
				deinitializeFreetype( ft );
				return nullptr;
			}
			else
			{
				output.pushMessage( CompilerErrorLevel::Error, m_data.name, "Could not fit %d out of %d glyphs into the atlas.\n", remaining, (int)glyphs.size() );
				destroyFont( font );
				deinitializeFreetype( ft );
				return nullptr;
			}
		}

		int width;
		int height;
		atlasPacker.getDimensions( width, height );
		if( width == 0 || height == 0 )
		{
			output.pushMessage( CompilerErrorLevel::Error, m_data.name, "Unable to determine atlas size." );
			destroyFont( font );
			deinitializeFreetype( ft );
			return nullptr;
		}

		const double emSize = m_data.font.size; // atlasPacker.getScale();
		const msdfgen::Range pxRange = atlasPacker.getPixelRange();
		//if( !fixedScale )
		//	printf( "Glyph size: %.9g pixels/em\n", config.emSize );
		//if( !fixedDimensions )
		//	printf( "Atlas dimensions: %d x %d\n", config.width, config.height );

		unsigned long long glyphSeed = 0;
		for( msdf_atlas::GlyphGeometry& glyph : glyphs )
		{
			glyphSeed *= 6364136223846793005ull;
			glyph.edgeColoring( msdfgen::edgeColoringInkTrap, 3.0, glyphSeed );
		}

		msdf_atlas::GeneratorAttributes generatorAttributes;
		generatorAttributes.config.overlapSupport = false;

		msdf_atlas::ImmediateAtlasGenerator< float, 4, msdf_atlas::mtsdfGenerator, msdf_atlas::BitmapAtlasStorage< byte,  4 > > generator( width, height );
		generator.setAttributes( generatorAttributes );
		generator.setThreadCount( 31 ); //config.threadCount );
		generator.generate( glyphs.data(), (int)glyphs.size() );

		const msdfgen::BitmapConstRef< byte,  4 > bitmap = (msdfgen::BitmapConstRef< byte, 4 >) generator.atlasStorage();

		m_fontSdfData.width		= (uint32)width;
		m_fontSdfData.height	= (uint32)height;

		const uintsize pixelDataWidth	= width * 4u;
		const uintsize pixelDataSize	= pixelDataWidth * height;

		m_fontSdfData.pixelData.assign( bitmap.pixels, pixelDataSize );

		const msdfgen::FontMetrics& fontMetrics = fontGeometry.getMetrics();

		m_fontSdfData.codepoints.reserve( glyphs.size() );
		for( const msdf_atlas::GlyphGeometry& glyph : glyphs )
		{
			ImUiFontCodepoint& codepoint = m_fontSdfData.codepoints.pushBack();

			double planeLeft;
			double planeBottom;
			double planeRight;
			double planeTop;
			glyph.getQuadPlaneBounds( planeLeft, planeBottom, planeRight, planeTop );

			double atlasLeft;
			double atlasBottom;
			double atlasRight;
			double atlasTop;
			glyph.getQuadAtlasBounds( atlasLeft, atlasBottom, atlasRight, atlasTop );

			codepoint.codepoint		= glyph.getCodepoint();
			codepoint.width			= float( (planeRight - planeLeft) * emSize );
			codepoint.height		= float( (planeTop - planeBottom) * emSize );
			codepoint.advance		= float( glyph.getAdvance() * emSize );
			codepoint.ascentOffset	= float( (fontMetrics.ascenderY - planeTop) * emSize );
			codepoint.uv.u0			= float( atlasLeft / width );
			codepoint.uv.v0			= float( atlasTop / height );
			codepoint.uv.u1			= float( atlasRight / width );
			codepoint.uv.v1			= float( atlasBottom / height );
		}

		FILE* f = fopen( "d:\\test.png", "wb" );
		if( f )
		{
			spng_ctx* png = spng_ctx_new( SPNG_CTX_ENCODER );
			spng_set_png_file( png, f );

			spng_ihdr pngHeader = {};
			pngHeader.width					= m_fontSdfData.width;
			pngHeader.height				= m_fontSdfData.height;
			pngHeader.bit_depth				= 8;
			pngHeader.color_type			= SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;

			spng_set_ihdr( png, &pngHeader );

			spng_encode_image( png, m_fontSdfData.pixelData.getData(), m_fontSdfData.pixelData.getLength(), SPNG_FMT_RAW, 0 );
			spng_ctx_free( png );

			//fwrite( m_fontSdfData.pixelData.getData(), 1u, m_fontSdfData.pixelData.getLength(), f );
			fclose( f );
		}

		destroyFont( font );
		deinitializeFreetype( ft );

		//m_fontSdfRevision = m_revision;
		return &m_fontSdfData;
#endif
	}

	void CompilerResourceData::convertSdfBitmapLine( ArrayView< uint32 > targetLine, const float* sourceLine, uintsize charSize )
	{
		//targetLine[ 0u ]			= convertSdfBitmapPixel( &sourceLine[ 0u ] );
		//targetLine[ 1u + charSize ]	= convertSdfBitmapPixel( &sourceLine[ (charSize - 1u) * 3u ] );

		for( uintsize x = 0u; x < charSize; ++x )
		{
			targetLine[ x ] = convertSdfBitmapPixel( &sourceLine[ x * 3u ] );
		}
	}

	inline float clampf( float n ) {
		return n >= 0.0f && n <= 1.0f ? n : float( n > 0.0f );
	}

	uint32 CompilerResourceData::convertSdfBitmapPixel( const float* source )
	{
		const uint32 r = byte( ~int( 255.5f - 255.0f * clampf( source[ 0u ] ) ) ); //uint32( clamp( (source[ 0u ] * 1.0f) + 127.0f, 0.0f, 255.0f ) );
		const uint32 g = byte( ~int( 255.5f - 255.0f * clampf( source[ 1u ] ) ) ); //uint32( clamp( (source[ 1u ] * 1.0f) + 127.0f, 0.0f, 255.0f ) );
		const uint32 b = byte( ~int( 255.5f - 255.0f * clampf( source[ 2u ] ) ) ); //uint32( clamp( (source[ 2u ] * 1.0f) + 127.0f, 0.0f, 255.0f ) );
		return (r << 24u) | (g << 16u) | (b << 8u) | 0xffu;
	}
}

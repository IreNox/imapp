#pragma once

#include "resource_theme.h"

#include <imapp/imapp.h>

#include <tiki/tiki_dynamic_string.h>

#include <tinyxml2.h>

struct ImAppImage;

namespace tiki
{
	class Path;
}

namespace imapp
{
	using namespace imui::toolbox;
	using namespace imui;
	using namespace tiki;
	using namespace tinyxml2;

	class ResourceTheme;

	enum class ResourceType
	{
		Image,
		Skin,
		Font,
		Theme,

		Count
	};

	enum class ResourceImageFormat
	{
		RAW,
		PNG,
		JPEG
	};

	struct ResourceFontUnicodeBlock
	{
		DynamicString			name;
		uint32					first	= 0u;
		uint32					last	= 0u;
	};

	ArrayView< const char* >	getResourceTypeStrings();
	bool						parseResourceType( ResourceType& type, const StringView& string );
	StringView					getResourceTypeString( ResourceType type );

	class Resource
	{
	public:

		using FontUnicodeBlockArray = DynamicArray< ResourceFontUnicodeBlock >;

								Resource();
								Resource( const StringView& name, ResourceType type );
								~Resource();

		bool					load( XMLElement* resourceNode );
		void					serialize( XMLElement* resourcesNode );
		void					remove();

		void					updateFileData( ImAppContext* imapp, const Path& packagePath, float time );

		StringView				getName() const { return m_name; }
		void					setName( const StringView& value );

		ResourceType			getType() const { return m_type; }

		StringView				getFileSourcePath() const { return m_fileSourcePath; }
		void					setFileSourcePath( const StringView& value );

		TikiHash32				getFileHash() const { return m_fileHash; }
		ArrayView< byte >		getFileData() const { return m_fileData; }

		bool					getImageAllowAtlas() const { return m_imageAllowAtlas; }
		void					setImageAllowAtlas( bool value );
		bool					getImageRepeat() const { return m_imageRepeat; }
		void					setImageRepeat( bool value );

		ImAppImage*				getImage() const { return m_image; }
		ArrayView< byte >		getImageData() const { return m_imageData; }
		uint32					getImageWidth() const { return m_imageWidth; }
		uint32					getImageHeight() const { return m_imageHeight; }

		float					getFontSize() const { return m_fontSize; }
		void					setFontSize( float value );
		bool					getFontIsScalable() const { return m_fontIsScalable; }
		void					setFontIsScalable( bool value );
		FontUnicodeBlockArray&	getFontUnicodeBlocks() { return m_fontBlocks; }

		StringView				getSkinImageName() const { return m_skinImageName; }
		void					setSkinImageName( const StringView& value );

		UiBorder&				getSkinBorder() { return m_skinBorder; }
		const UiBorder&			getSkinBorder() const { return m_skinBorder; }

		ResourceTheme&			getTheme() { return m_theme; }
		const ResourceTheme&	getTheme() const { return m_theme; }

		uint32					getRevision() const { return m_revision; }
		void					increaseRevision();

	private:

		using ByteArray = DynamicArray< byte >;

		uint32					m_revision			= 0u;

		DynamicString			m_name;
		ResourceType			m_type				= ResourceType::Image;

		XMLElement*				m_xml				= nullptr;

		DynamicString			m_fileSourcePath;
		ByteArray				m_fileData;
		TikiHash32				m_fileHash			= 0u;
		float					m_fileCheckTime		= -1000.0f;

		ResourceImageFormat		m_imageFormat;
		ByteArray				m_imageData;
		uint32					m_imageWidth		= 0u;
		uint32					m_imageHeight		= 0u;
		ImAppContext*			m_imapp				= nullptr;
		ImAppImage*				m_image				= nullptr;
		bool					m_imageAllowAtlas	= true;
		bool					m_imageRepeat		= false;

		float					m_fontSize			= 0.0f;
		bool					m_fontIsScalable	= false;
		FontUnicodeBlockArray	m_fontBlocks;

		DynamicString			m_skinImageName;
		UiBorder				m_skinBorder;

		ResourceTheme			m_theme;

		bool					loadImageXml();
		bool					loadSkinXml();
		bool					loadFontXml();
		void					serializeImageXml();
		void					serializeSkinXml();
		void					serializeFontXml();

		void					updateImageFileData( ImAppContext* imapp );
		void					updateFontFileData( ImAppContext* imapp );
	};
}

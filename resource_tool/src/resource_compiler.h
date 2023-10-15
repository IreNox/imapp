#pragma once

#include "resource.h"

#include <tiki/tiki_dynamic_string.h>
#include <tiki/tiki_hash_map.h>

namespace tiki
{
	class StringView;
}

namespace imapp
{
	using namespace tiki;

	struct Thread;
	class ResourcePackage;

	enum class ResourceCompilerOutputLevel
	{
		Info,
		Warning,
		Error
	};

	struct ResourceCompilerOutput
	{
		ResourceCompilerOutputLevel	level;
		DynamicString				message;
		DynamicString				resourceName;
	};

	class ResourceCompiler
	{
	public:

		using OutputView = ArrayView< ResourceCompilerOutput >;

							ResourceCompiler( ResourcePackage& package );
							~ResourceCompiler();

		bool				startCompile( const StringView& outputPath );
		bool				isRunning() const;
		bool				getResult() const { return m_result; }

		OutputView			getOutputs() const { return m_outputs; }

	private:

		using ByteArray = DynamicArray< byte >;

		struct ImageData
		{
			uint16			width			= 0u;
			uint16			height			= 0u;
			bool			allowAtlas		= false;
		};

		struct FontData
		{
			float			size			= 0u;
		};

		struct ThemeData
		{
			ResourceTheme*	theme	= nullptr;
		};

		struct ResourceData
		{
			ResourceType	type			= ResourceType::Count;
			ImUiHash		dataHash		= 0u;
			ByteArray		data;

			ImageData		image;
			FontData		font;
			ThemeData		theme;
		};

		struct AtlasImage
		{
			uint16			x		= 0u;
			uint16			y		= 0u;
			uint16			width	= 0u;
			uint16			height	= 0u;
		};

		using ResourceMap = HashMap< DynamicString, ResourceData >;
		using AtlasImageMap = HashMap< DynamicString, AtlasImage >;
		using OutputArray = DynamicArray< ResourceCompilerOutput >;

		ResourcePackage&	m_package;

		ResourceMap			m_resources;
		ByteArray			m_buffer;

		uint16				m_atlasWidth	= 0u;
		uint16				m_atlasHeight	= 0u;
		AtlasImageMap		m_atlasImages;
		ByteArray			m_atlasData;

		Thread*				m_thread		= nullptr;
		bool				m_result		= false;
		OutputArray			m_outputs;

		static void			staticRunCompileThread( void* arg );
		void				runCompileThread();

		bool				updateImageAtlas();
	};
}
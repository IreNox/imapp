#pragma once

#include "resource.h"

#include <tiki/tiki_dynamic_string.h>
#include <tiki/tiki_hash_map.h>
#include <tiki/tiki_path.h>

#include <imapp/../../src/imapp_res_pak.h>

namespace tiki
{
	class StringView;
}

namespace imapp
{
	using namespace tiki;

	class ResourcePackage;
	class Thread;

	enum class ResourceErrorLevel
	{
		Info,
		Warning,
		Error
	};

	struct ResourceCompilerOutput
	{
		ResourceErrorLevel	level;
		DynamicString		message;
		DynamicString		resourceName;
	};

	class ResourceCompiler
	{
	public:

		using OutputView = ArrayView< ResourceCompilerOutput >;

							ResourceCompiler();
							~ResourceCompiler();

		bool				startCompile( const StringView& outputPath, const ResourcePackage& package );
		bool				isRunning();
		bool				getResult() const { return m_result; }

		OutputView			getOutputs() const { return m_outputs; }

	private:

		using ByteArray = DynamicArray< byte >;

		struct ImageData
		{
			uint32			width			= 0u;
			uint32			height			= 0u;
			bool			allowAtlas		= false;
			bool			isAtlas			= false;
			ByteArray		imageData;
		};

		struct FontData
		{
			float			size			= 0u;
		};


		struct SkinData
		{
			DynamicString	imageName;
			UiBorder		border;
		};

		struct ThemeData
		{
			ResourceTheme*	theme	= nullptr;
		};

		struct ResourceData
		{
			ResourceType	type			= ResourceType::Count;
			DynamicString	name;
			ImUiHash		fileHash		= 0u;
			ByteArray		fileData;

			ByteArray		compiledData;

			ImageData		image;
			FontData		font;
			SkinData		skin;
			ThemeData		theme;
		};

		struct CompiledResource
		{
			ImAppResPakType		type;
			const ResourceData*	data;
			uint32				nameOffset;
			uint8				nameLength;
			uint16				refIndex;
		};

		struct AtlasImage
		{
			uint32			x		= 0u;
			uint32			y		= 0u;
			uint32			width	= 0u;
			uint32			height	= 0u;
		};

		using ResourceMap = HashMap< DynamicString, ResourceData >;
		using AtlasImageMap = HashMap< DynamicString, AtlasImage >;
		using OutputArray = DynamicArray< ResourceCompilerOutput >;
		using CompiledResourceArray = DynamicArray< CompiledResource >;
		using ResourceTypeIndexArray = StaticArray< DynamicArray< uint16 >, ImAppResPakType_MAX >;
		using ResourceTypeIndexMap = StaticArray< HashMap< DynamicString, uint16 >, ImAppResPakType_MAX >;

		Path				m_outputPath;
		ResourceMap			m_resources;
		ByteArray			m_buffer;

		ResourceData		m_atlasData;
		AtlasImageMap		m_atlasImages;

		Thread*				m_thread		= nullptr;
		bool				m_result		= false;
		OutputArray			m_outputs;

		static void			staticRunCompileThread( void* arg );
		void				runCompileThread();

		bool				updateImageAtlas();
		void				prepareCompiledResources( CompiledResourceArray& compiledResources, ResourceTypeIndexMap& resourceIndexMapping, ResourceTypeIndexArray& resourcesByType );
		void				writeResourceNames( CompiledResourceArray& compiledResources );
		void				writeResourceHeaders( uint32 resourcesOffset, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping );
		void				writeResourceData( uint32 resourcesOffset, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping );

		template< typename T >
		T&					getBufferData( uint32 offset );
		template< typename T >
		T&		 			getBufferArrayElement( uint32 offset, uintsize index );
		template< typename T >
		uint32				preallocateToBuffer( uintsize alignment = 1u );
		template< typename T >
		uint32				preallocateArrayToBuffer( uintsize length, uintsize alignment = 1u );
		template< typename T >
		uint32				writeToBuffer( const T& value, uintsize alignment = 1u );
		template< typename T >
		uint32				writeArrayToBuffer( const ArrayView< T >& array, uintsize alignment = 1u );

		bool				findResourceIndex( uint16& target, const ResourceTypeIndexMap& mapping, ImAppResPakType type, const DynamicString& name, const StringView& resourceName );

		void				pushOutput( ResourceErrorLevel level, const StringView& resourceName, const char* format, ... );
	};
}
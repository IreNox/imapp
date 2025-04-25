#pragma once

#include "compiler_output.h"
#include "compiler_resource_data.h"
#include "resource.h"

#include <tiki/tiki_dynamic_string.h>
#include <tiki/tiki_hash_map.h>
#include <tiki/tiki_path.h>
#include <tiki/tiki_static_array.h>

#include <imapp/../../src/imapp_res_pak.h>

namespace imapp
{
	using namespace tiki;

	class CompilerResourceData;
	class ResourcePackage;
	class Thread;

	class Compiler
	{
	public:

								Compiler();
								~Compiler();

		void					reset();

		bool					startCompile( const ResourcePackage& package );
		void					waitForCompile();
		bool					isRunning();

		const CompilerOutput&	getOutput() const { return m_output; }

	private:

		struct CompiledResource
		{
			ImAppResPakType				type;
			const CompilerResourceData*	data;
			const CompilerFontData*		fontData;
			uint32						nameOffset;
			uint8						nameLength;
			uint16						refIndex;
		};

		struct AtlasImage
		{
			uint32						x		= 0u;
			uint32						y		= 0u;
			uint32						width	= 0u;
			uint32						height	= 0u;
		};

		using ResourceMap = HashMap< DynamicString, CompilerResourceData* >;
		using AtlasImageMap = HashMap< DynamicString, AtlasImage >;
		using CompiledResourceArray = DynamicArray< CompiledResource >;
		using ResourceTypeIndexArray = StaticArray< DynamicArray< uint16 >, ImAppResPakType_MAX >;
		using ResourceTypeIndexMap = StaticArray< HashMap< DynamicString, uint16 >, ImAppResPakType_MAX >;

		class BinaryBuffer
		{
		public:

			using ByteView = ConstArrayView< byte >;

			void					clear();

			ByteView				getData() const { return m_buffer; }
			uintsize				getLength() const { return m_buffer.getLength(); }

			template< typename T >
			T&						getBufferData( uint32 offset );
			template< typename T >
			T&		 				getBufferArrayElement( uint32 offset, uintsize index );
			template< typename T >
			uint32					preallocateToBuffer( uintsize alignment = 1u );
			template< typename T >
			uint32					preallocateArrayToBuffer( uintsize length, uintsize alignment = 1u );
			template< typename T >
			uint32					writeToBuffer( const T& value, uintsize alignment = 1u );
			template< typename T >
			uint32					writeArrayToBuffer( const ArrayView< T >& array, uintsize alignment = 1u );

		private:

			using ByteArray = DynamicArray< byte >;

			ByteArray			m_buffer;
		};

		DynamicString			m_packageName;
		Path					m_outputPath;
		bool					m_outputCode;
		ResourceMap				m_resources;
		BinaryBuffer			m_buffer;

		CompilerResourceData	m_atlasData;
		AtlasImageMap			m_atlasImages;

		Thread*					m_thread		= nullptr;
		bool					m_result		= false;
		CompilerOutput			m_output;

		static void				staticRunCompileThread( void* arg );
		void					runCompileThread();

		bool					updateImageAtlas();
		void					prepareCompiledResources( CompiledResourceArray& compiledResources, ResourceTypeIndexMap& resourceIndexMapping, ResourceTypeIndexArray& resourcesByType );
		void					writeResourceNames( CompiledResourceArray& compiledResources );
		void					writeResourceHeaders( uint32 resourcesOffset, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping );
		void					writeResourceData( uint32 resourcesOffset, const CompiledResourceArray& compiledResources, const ResourceTypeIndexMap& resourceIndexMapping );

		void					writeBinaryFile();
		void					writeCodeFile();

		bool					findResourceIndex( uint16& target, const ResourceTypeIndexMap& mapping, ImAppResPakType type, const DynamicString& name, const StringView& resourceName ) const;
	};
}
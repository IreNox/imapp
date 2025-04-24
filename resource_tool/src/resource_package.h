#pragma once

#include "resource_helpers.h"

#include <tiki/tiki_dynamic_string.h>
#include <tiki/tiki_path.h>

#include <tinyxml2.h>

struct ImAppContext;

namespace imapp
{
	using namespace tiki;
	using namespace tinyxml2;

	class Resource;
	enum class ResourceType;

	class ResourcePackage
	{
	public:

		using ResourceView = ArrayView< Resource* >;

		ResourcePackage();
		~ResourcePackage();

		bool					load( const StringView& filename );

		bool					hasPath() const;
		bool					save();
		bool					saveAs( const StringView& filename );

		void					updateFileData( double time );

		const Path&				getPath() const { return m_path; }

		DynamicString&			getName() { return m_name; }
		const DynamicString&	getName() const { return m_name; }
		DynamicString&			getOutputPath() { return m_outputPath; }
		const DynamicString&	getOutputPath() const { return m_outputPath; }
		bool&					getExportCode() { return m_exportCode; }
		const bool&				getExportCode() const { return m_exportCode; }

		Resource&				addResource( const StringView& name, ResourceType type );
		Resource&				getResource( uintsize index );
		void					removeResource( uintsize index );
		uintsize				getResourceCount() const;
		ResourceView			getResources() const { return m_resources; }

		Resource*				findResource( const StringView& name );

		uint32					getRevision() const;
		void					increaseRevision() { m_revision++; }

	private:

		using ResourceArray = DynamicArray< Resource* >;

		uint32					m_revision	= 0u;

		Path					m_path;
		XMLDocument				m_xml;

		DynamicString			m_name;
		DynamicString			m_outputPath;
		bool					m_exportCode = false;

		ResourceArray			m_resources;
	};
}

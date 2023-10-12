#pragma once

namespace tiki
{
	class StringView;
}

namespace imapp
{
	using namespace tiki;

	class ResourcePackage;

	class ResourceCompiler
	{
	public:

							ResourceCompiler( ResourcePackage& package );
							~ResourceCompiler();

		bool				compile( const StringView& outputPath );

	private:

		using ByteArray = DynamicArray< byte >;

		ResourcePackage&	m_package;
		ByteArray			m_buffer;
	};
}
#pragma once

#include "resource_types.h"

#include <tiki/tiki_dynamic_string.h>

namespace imapp
{
	using namespace tiki;

	enum class CompilerErrorLevel
	{
		Info,
		Warning,
		Error
	};

	struct CompilerMessage
	{
		CompilerErrorLevel	level;
		DynamicString		text;
		DynamicString		resourceName;
	};

	class CompilerOutput
	{
	public:

		using MessageView = ConstArrayView< CompilerMessage >;

							CompilerOutput();
							~CompilerOutput();

		bool				hasError() const { return m_hasError; }
		MessageView			getMessages() const { return m_messages; }

		void				clear();
		void				pushMessage( CompilerErrorLevel level, const StringView& resourceName, const char* format, ... );

	private:

		using MessageArray = DynamicArray< CompilerMessage >;

		bool				m_hasError;
		MessageArray		m_messages;
	};
}
#include "compiler_output.h"

namespace imapp
{
	CompilerOutput::CompilerOutput()
	{
		clear();
	}

	CompilerOutput::~CompilerOutput()
	{
	}

	void CompilerOutput::clear()
	{
		m_hasError = false;
		m_messages.clear();
	}

	void CompilerOutput::pushMessage( CompilerErrorLevel level, const StringView& resourceName, const char* format, ... )
	{
		CompilerMessage& message = m_messages.pushBack();
		message.level			= CompilerErrorLevel::Error;
		message.resourceName	= resourceName;

		va_list args;
		va_start( args, format );
		message.text			= DynamicString::formatArgs( format, args );
		va_end( args );

		if( level >= CompilerErrorLevel::Error )
		{
			m_hasError = true;
		}
	}
}

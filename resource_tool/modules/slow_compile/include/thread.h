#pragma once

#include <tiki/tiki_string_view.h>

namespace imapp
{
	using namespace tiki;

	class Thread;

	using ThreadFunc = void (*)( void* arg );

	Thread*		createThread( const StringView& name, ThreadFunc, void* arg );
	bool		isThreadRunning( const Thread* thread );
	void		destroyThread( Thread* thread );
}
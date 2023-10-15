#pragma once

#include <tiki/tiki_string_view.h>

namespace imapp
{
	using namespace tiki;

	struct Thread;
	struct Mutex;

	Thread*		createThread( const StringView& name, void(*threadFunc)( void* arg ), void* arg );
	bool		isThreadRunning( const Thread* thread );
	void		destroyThread( Thread* thread );
}
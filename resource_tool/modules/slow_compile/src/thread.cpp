#include "thread.h"

#include <thread>

namespace imapp
{
	Thread* imapp::createThread( const StringView& name, void(*threadFunc)(void* arg), void* arg )
	{
		return (Thread*)new std::thread( threadFunc, arg );;
	}

	bool isThreadRunning( const Thread* thread )
	{
		const std::thread* stdThread = (const std::thread*)thread;
		return stdThread->joinable();
	}

	void destroyThread( Thread* thread )
	{
		std::thread* stdThread = (std::thread*)thread;
		stdThread->join();
		delete stdThread;
	}
}

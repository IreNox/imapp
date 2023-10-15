#include "thread.h"

#include <atomic>
#include <thread>

namespace imapp
{
	class Thread
	{
	public:

							Thread( ThreadFunc func, void* arg );
							~Thread();

		bool				isRunning() const { return m_running; }

	private:

		std::atomic< bool >	m_running;
		std::thread			m_thread;

		ThreadFunc			m_func;
		void*				m_arg;

		void				run();
	};

	Thread::Thread( ThreadFunc func, void* arg )
		: m_func( func )
		, m_arg( arg )
	{
		m_running = true;
		m_thread = std::thread( &Thread::run, this );
	}

	Thread::~Thread()
	{
		m_thread.join();
	}

	void Thread::run()
	{
		m_func( m_arg );
		m_running = false;
	}

	Thread* imapp::createThread( const StringView& name, ThreadFunc func, void* arg )
	{
		return new Thread( func, arg );
	}

	bool isThreadRunning( const Thread* thread )
	{
		return thread->isRunning();
	}

	void destroyThread( Thread* thread )
	{
		delete thread;
	}
}

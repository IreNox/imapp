#pragma once

#include "resource_package.h"

#include <tiki/tiki_dynamic_string.h>

struct ImAppContext;

namespace imui
{
	class UiSurface;
}

namespace imapp
{
	using namespace imui;
	using namespace tiki;

	class ResourceTool
	{
	public:

						ResourceTool();

		void			load( const char* filename );

		void			doUi( ImAppContext* imapp, UiSurface& surface );

	private:

		enum class State
		{
			Home,
			New,
			DeleteConfirm,
			Error
		};

		DynamicString	m_packagePath;
		ResourcePackage	m_package;

		DynamicString	m_errorMessage;

		State			m_state				= State::Home;

		size_t			m_selecedResource	= 0u;

		void			doState( UiSurface& surface );
		void			doStateNew( UiSurface& surface );
		void			doStateDeleteConfirm( UiSurface& surface );
		void			doStateError( UiSurface& surface );

		void			showError( const char* format, ... );
	};
}

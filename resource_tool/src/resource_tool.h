#pragma once

#include "resource_package.h"

#include <tiki/tiki_dynamic_string.h>

struct ImAppContext;

namespace imui
{
	class UiSurface;

	namespace toolbox
	{
		class UiToolboxWindow;
	}
}

namespace imapp
{
	using namespace imui;
	using namespace imui::toolbox;
	using namespace tiki;

	class ResourceTool
	{
	public:

						ResourceTool();

		void			load( const char* filename );

		void			doUi( ImAppContext* imapp, UiSurface& surface );

	private:

		enum class PopupState
		{
			Home,
			New,
			DeleteConfirm,
			Error
		};

		DynamicString	m_packagePath;
		ResourcePackage	m_package;

		PopupState		m_popupState		= PopupState::Home;
		DynamicString	m_errorMessage;

		size_t			m_selecedResource	= (size_t)-1;

		void			doPopupState( UiSurface& surface );
		void			doPupupStateNew( UiSurface& surface );
		void			doPopupStateDeleteConfirm( UiSurface& surface );
		void			doPopupStateError( UiSurface& surface );

		void			doView( UiToolboxWindow& window );
		void			doViewImage( UiToolboxWindow& window, Resource& resource );
		void			doViewSkin( UiToolboxWindow& window, Resource& resource );
		void			doViewConfig( UiToolboxWindow& window, Resource& resource );

		void			showError( const char* format, ... );
	};
}

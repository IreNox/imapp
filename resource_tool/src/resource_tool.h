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

		void				load( const char* filename );

		void				doUi( ImAppContext* imapp, UiSurface& surface );

	private:

		enum class PopupState
		{
			Home,
			New,
			DeleteConfirm,
			Error
		};

		class ImageViewWidget : public UiWidget
		{
		public:

							ImageViewWidget( UiToolboxWindow& window );
							~ImageViewWidget();

			UiWidget&		getContent() { return m_scrollContent; }

			float			getZoom() const { return m_state->zoom; }

		private:

			struct ScrollState
			{
				UiPos		lastMousePos;
				UiPos		offset;
				float		zoom		= 1.0f;
			};

			UiWidget		m_scrollArea;
			UiWidget		m_scrollContent;

			ScrollState*	m_state;
		};

		struct SkinState
		{
			uintsize		selectedImage;
		};

		DynamicString		m_packagePath;
		ResourcePackage		m_package;

		const ImAppImage*	m_icon				= nullptr;

		PopupState			m_popupState		= PopupState::Home;
		DynamicString		m_errorMessage;

		size_t				m_selecedResource	= (size_t)-1;

		void				doPopupState( UiSurface& surface );
		void				doPupupStateNew( UiSurface& surface );
		void				doPopupStateDeleteConfirm( UiSurface& surface );
		void				doPopupStateError( UiSurface& surface );

		void				doView( UiToolboxWindow& window );
		void				doViewImage( UiToolboxWindow& window, Resource& resource );
		void				doViewSkin( UiToolboxWindow& window, Resource& resource );
		void				doViewConfig( UiToolboxWindow& window, Resource& resource );

		void				handleDrop( const char* dropData );

		void				showError( const char* format, ... );
	};
}

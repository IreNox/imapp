#pragma once

#include "resource.h"
#include "resource_compiler.h"
#include "resource_package.h"

#include <tiki/tiki_dynamic_string.h>
#include <tiki/tiki_static_array.h>

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

		using TypeNameArray = StaticArray< DynamicArray< UiStringView >, (uintsize)ResourceType::Count >;

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

							ImageViewWidget( ImAppContext* imapp, UiToolboxWindow& window );
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

		struct ThemeState
		{
			float			maxWidth;
		};

		ResourcePackage		m_package;
		ResourceCompiler	m_compiler;

		bool				m_autoCompile			= false;
		uint32				m_lastCompileRevision	= 0u;

		PopupState			m_popupState			= PopupState::Home;
		DynamicString		m_errorMessage;

		size_t				m_selecedEntry			= (size_t)-1;

		TypeNameArray		m_resourceNamesByType;

		void				update( ImAppContext* imapp, float time );

		void				doPopupState( UiSurface& surface );
		void				doPupupStateNew( UiSurface& surface );
		void				doPopupStateDeleteConfirm( UiSurface& surface );
		void				doPopupStateError( UiSurface& surface );

		void				doView( ImAppContext* imapp, UiToolboxWindow& window );
		void				doViewPackage( UiToolboxWindow& window );
		void				doViewImage( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource );
		void				doViewSkin( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource );
		void				doViewTheme( UiToolboxWindow& window, Resource& resource );

		bool				doFloatTextEdit( UiToolboxWindow& window, float& value );
		StringView			doResourceSelect( UiToolboxWindow& window, ResourceType type, const StringView& selectedResourceName );

		void				handleDrop( const char* dropData );

		void				showError( const char* format, ... );

		void				updateResourceNamesByType();
	};
}

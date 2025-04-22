#pragma once

#include "compiler.h"
#include "resource.h"
#include "resource_package.h"

#include <tiki/tiki_dynamic_string.h>
#include <tiki/tiki_flags.h>
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

		bool				handleArgs( int argc, char* argv[], bool& shutdown );

		void				doUi( ImAppContext* imapp, ImAppWindow* appWindow, UiToolboxWindow& uiWindow );

	private:

		using TypeNameArray = StaticArray< DynamicArray< const char* >, (uintsize)ResourceType::Count >;

		enum class PopupState
		{
			Home,
			New,
			DeleteConfirm,
			Error
		};

		enum class Notifications
		{
			Loaded,
			Saved,
			Compiled,

			Count
		};

		using NotificationFlags = Flags8< Notifications >;

		class ImageViewWidget : public UiWidget
		{
		public:

								ImageViewWidget( ImAppContext* imapp, UiToolboxWindow& window );
								~ImageViewWidget();

			UiWidget&			getContent() { return m_scrollContent; }

			float				getZoom() const { return m_state->zoom; }

		private:

			struct ScrollState
			{
				UiPos			lastMousePos;
				UiPos			offset;
				float			zoom		= 1.0f;

				UiColor			bgColor		= UiColor::TransparentBlack;
			};

			UiWidget			m_scrollArea;
			UiWidget			m_scrollContent;

			const ImUiImage*	m_bgImage;
			ScrollState*		m_state;

			void				doBackgroundButton( const UiColor& color );
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
		Compiler			m_compiler;

		bool				m_autoCompile			= false;
		bool				m_wasCompiling			= false;
		uint32				m_lastCompileRevision	= 0u;

		PopupState			m_popupState			= PopupState::Home;
		DynamicString		m_errorMessage;
		NotificationFlags	m_notifications;

		size_t				m_selecedEntry			= 0;
		bool				m_overrideSelectedEntry	= true;

		TypeNameArray		m_resourceNamesByType;

		bool				load( const char* filename );

		void				update( ImAppWindow* appWindow, double time );

		void				doPopups( UiSurface& surface );
		void				doPupupStateNew( UiSurface& surface );
		void				doPopupStateDeleteConfirm( UiSurface& surface );
		void				doPopupStateError( UiSurface& surface );

		void				doNotifications( UiSurface& surface );

		void				doView( ImAppContext* imapp, UiToolboxWindow& window );
		void				doViewPackage( UiToolboxWindow& window );
		void				doViewImage( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource );
		void				doViewFont( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource );
		void				doViewSkin( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource );
		void				doViewTheme( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource );

		bool				doUIntTextEdit( UiToolboxWindow& window, uint32& value );
		bool				doFloatTextEdit( UiToolboxWindow& window, float& value, uintsize decimalNumbers );
		StringView			doResourceSelect( UiToolboxWindow& window, ResourceType type, const StringView& selectedResourceName );

		void				handleDrop( const char* dropData );

		void				showError( const char* format, ... );

		void				updateResourceNamesByType();
	};
}

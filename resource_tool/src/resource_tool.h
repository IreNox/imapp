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

		ResourcePackage		m_package;
		ResourceCompiler	m_compiler;

		const ImAppImage*	m_icon				= nullptr;

		PopupState			m_popupState		= PopupState::Home;
		DynamicString		m_errorMessage;

		size_t				m_selecedEntry	= (size_t)-1;

		TypeNameArray		m_resourceNamesByType;

		void				doPopupState( UiSurface& surface );
		void				doPupupStateNew( UiSurface& surface );
		void				doPopupStateDeleteConfirm( UiSurface& surface );
		void				doPopupStateError( UiSurface& surface );

		void				doView( UiToolboxWindow& window );
		void				doViewPackage( UiToolboxWindow& window );
		void				doViewImage( UiToolboxWindow& window, Resource& resource );
		void				doViewSkin( UiToolboxWindow& window, Resource& resource );
		void				doViewConfig( UiToolboxWindow& window, Resource& resource );

		bool				doFloatTextEdit( UiToolboxWindow& window, float& value );
		StringView			doResourceSelect( UiToolboxWindow& window, ResourceType type, const StringView& selectedResourceName );

		void				handleDrop( const char* dropData );

		void				showError( const char* format, ... );

		void				updateResourceNamesByType();
	};
}

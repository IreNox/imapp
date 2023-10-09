#include "resource_tool.h"

#include "resource.h"
#include "resource_helpers.h"

#include <imapp/imapp.h>
#include <imui/imui_cpp.h>

#include <cstdarg>

namespace imapp
{
	using namespace imui;
	using namespace imui::toolbox;

	ResourceTool::ResourceTool()
	{
	}

	void ResourceTool::load( const char* filename )
	{
		if( !m_package.load( (RtStr)filename ) )
		{
			showError( "Failed to load '%s'.", filename );
		}
	}

	void ResourceTool::doUi( ImAppContext* imapp, UiSurface& surface )
	{
		m_package.updateFileData( imapp, surface.getTime() );

		doPopupState( surface );

		UiToolboxWindow window( surface, "main", surface.getRect(), 1 );

		UiWidgetLayoutVertical mainLayout( window, 8.0f );
		mainLayout.setStretch( UiSize::One );
		mainLayout.setMargin( UiBorder( 8.0f ) );

		{
			UiWidgetLayoutHorizontal titleLayout( window, 8.0f );

			if( window.buttonLabel( "Open" ) )
			{

			}

			if( window.buttonLabel( "Save" ) )
			{
				if( m_package.hasPath() )
				{
					m_package.save();
				}
			}

			window.label( "Project Name" );
		}

		{
			UiWidgetLayoutHorizontal workLayout( window, 8.0f );
			workLayout.setStretch( UiSize::One );

			{
				UiWidgetLayoutVertical leftLayout( window );
				leftLayout.setStretch( UiSize::Vertical );

				{
					UiToolboxList list( window, 20.0f, m_package.getResourceCount() );
					list.setStretch( UiSize::One );
					list.setMinWidth( 150.0f );

					for( uintsize i = 0; i < m_package.getResourceCount(); ++i )
					{
						const Resource& resource = m_package.getResource( i );

						list.nextItem();

						window.label( (RtStr)resource.getName() );
					}

					m_selecedResource = list.getSelectedIndex();
				}

				{
					UiWidgetLayoutHorizontal buttonsLayout( window, 8.0f );
					buttonsLayout.setStretch( UiSize::Horizontal );

					UiToolboxButtonLabel plusButton( window, "+" );
					plusButton.setStretch( UiSize::Horizontal );
					if( plusButton.end() )
					{
						m_popupState = PopupState::New;
					}

					UiToolboxButtonLabel minusButton( window, "-" );
					minusButton.setStretch( UiSize::Horizontal );
					if( minusButton.end() )
					{
						m_popupState = PopupState::DeleteConfirm;
					}
				}
			}

			{
				UiWidget viewWidget( window );
				viewWidget.setStretch( UiSize::One );
				viewWidget.setLayoutVertical();

				doView( window );
			}
		}

	}

	void ResourceTool::doPopupState( UiSurface& surface )
	{
		switch( m_popupState )
		{
		case ResourceTool::PopupState::Home:
			break;

		case ResourceTool::PopupState::New:
			doPupupStateNew( surface );
			break;

		case ResourceTool::PopupState::DeleteConfirm:
			doPopupStateDeleteConfirm( surface );
			break;

		case ResourceTool::PopupState::Error:
			doPopupStateError( surface );
			break;
		}
	}

	void ResourceTool::doPupupStateNew( UiSurface& surface )
	{
		UiToolboxPopup popup( surface );

		UiStringView name;
		size_t typeIndex;
		{
			UiWidgetLayoutVertical layout( popup );
			layout.setMinWidth( 250.0f );

			popup.label( "Name:" );
			name = popup.textEditState( 64u );
			popup.spacer( 1.0f, 4.0f );

			popup.label( "Type:" );
			const ArrayView< StringView > items = getResourceTypeStrings();
			// HACK: really ugly but works. need a was to convert to UiStringView in a proper way.
			typeIndex = popup.dropDown( (const UiStringView*)items.getData(), items.getLength() );
			popup.spacer( 1.0f, 4.0f );
		}

		bool ok = true;
		if( name.isEmpty() )
		{
			popup.label( "No name set!" );
			ok = false;
		}
		if( typeIndex >= ImAppResPakType_MAX )
		{
			popup.label( "No type set!" );
			ok = false;
		}

		const UiStringView buttons[] = { "Ok", "Cancel" };
		const size_t buttonIndex = popup.end( buttons, TIKI_ARRAY_COUNT( buttons ) );
		if( buttonIndex == 0u && ok )
		{
			m_package.addResource( (RtStr)name, (ResourceType)typeIndex );
			m_popupState = PopupState::Home;
		}
		else if( buttonIndex == 1u )
		{
			m_popupState = PopupState::Home;
		}
	}

	void ResourceTool::doPopupStateDeleteConfirm( UiSurface& surface )
	{

	}

	void ResourceTool::doPopupStateError( UiSurface& surface )
	{
		UiToolboxPopup popup( surface );

		{
			UiWidget padding( popup );
			padding.setPadding( UiBorder( 4.0f ) );

			popup.label( (RtStr)m_errorMessage );
		}

		const UiStringView buttons[] = { "Ok" };
		const size_t buttonIndex = popup.end( buttons, TIKI_ARRAY_COUNT( buttons ) );
		if( buttonIndex == 0u )
		{
			m_popupState = PopupState::Home;
		}
	}

	void ResourceTool::doView( UiToolboxWindow& window )
	{
		if( m_selecedResource >= m_package.getResourceCount() )
		{
			window.label( "No resource selected." );
			return;
		}

		Resource& resource = m_package.getResource( m_selecedResource );

		window.label( "Name:" );
		{
			UiWidgetLayoutHorizontal pathLayout( window, 8.0f );
			pathLayout.setStretch( UiSize::Horizontal );

			const RtStr newName = window.textEditState( 256u, (RtStr)resource.getName() );
			if( newName != resource.getName() &&
				window.buttonLabel( "Change" ) )
			{
				resource.setName( newName );
			}
		}

		switch( resource.getType() )
		{
		case ResourceType::Image:
			doViewImage( window, resource );
			break;

		case ResourceType::Skin:
			doViewSkin( window, resource );
			break;

		case ResourceType::Config:
			doViewConfig( window, resource );
			break;
		}
	}

	void ResourceTool::doViewImage( UiToolboxWindow& window, Resource& resource )
	{
		window.label( "Path:" );
		{
			UiWidgetLayoutHorizontal pathLayout( window, 8.0f );
			pathLayout.setStretch( UiSize::Horizontal );

			const RtStr newPath = window.textEditState( 256u, (RtStr)resource.getImageSourcePath() );
			if( newPath != resource.getImageSourcePath() &&
				window.buttonLabel( "Change" ) )
			{
				resource.setImageSourcePath( newPath );
			}
		}

		const ImUiTexture imageTexture = ImAppImageGetTexture( resource.getImage() );

		ImageViewWidget imageView( window );

		UiWidget image( window );
		image.setFixedSize( (UiSize)imageTexture.size * imageView.getZoom() );

		image.drawWidgetTexture( imageTexture );
	}

	void ResourceTool::doViewSkin( UiToolboxWindow& window, Resource& resource )
	{
		DynamicArray< UiStringView > imageResourceNames;
		imageResourceNames.reserve( m_package.getResourceCount() );

		for( uintsize i = 0; i < m_package.getResourceCount(); ++i )
		{
			const Resource& resource = m_package.getResource( i );
			if( resource.getType() != ResourceType::Image )
			{
				continue;
			}

			imageResourceNames.pushBack( (RtStr)resource.getName() );
		}

		const uintsize selectIndex = window.dropDown( imageResourceNames.getData(), imageResourceNames.getLength() );
		if( selectIndex < imageResourceNames.getLength() )
		{
			const StringView selectedName = (RtStr)imageResourceNames[ selectIndex ];
			if( selectedName != resource.getSkinImageName() )
			{
				resource.setSkinImageName( selectedName );
			}
		}

		Resource* imageResource = m_package.findResource( resource.getSkinImageName() );
		if( imageResource )
		{
			const ImUiTexture imageTexture = ImAppImageGetTexture( imageResource->getImage() );

			window.label( "Top:" );
			window.slider( resource.getSkinBorder().top, 0.0f, imageTexture.size.height);
			window.label( "Left:" );
			window.slider( resource.getSkinBorder().left, 0.0f, imageTexture.size.width );
			window.label( "Bottom:" );
			window.slider( resource.getSkinBorder().bottom, 0.0f, imageTexture.size.height );
			window.label( "Right:" );
			window.slider( resource.getSkinBorder().right, 0.0f, imageTexture.size.width );

			ImageViewWidget imageView( window );

			UiWidget image( window );
			image.setFixedSize( (UiSize)imageTexture.size * imageView.getZoom() );

			image.drawWidgetTexture( imageTexture );
		}
		else
		{
			window.labelFormat( "No Image selected!" );
		}
	}

	void ResourceTool::doViewConfig( UiToolboxWindow& window, Resource& resource )
	{

	}

	void ResourceTool::showError( const char* format, ... )
	{
		va_list args;
		va_start( args, format );
		m_errorMessage = DynamicString::formatArgs( format, args );
		va_end( args );

		m_popupState = PopupState::Error;
	}

	ResourceTool::ImageViewWidget::ImageViewWidget( UiToolboxWindow& window )
		: UiWidget( window )
		, m_scrollArea( window )
		, m_scrollContent( window )
	{
		setStretch( UiSize::One );

		m_state = newState< ScrollState >();

		m_scrollArea.setStretch( UiSize::One );
		m_scrollArea.setLayoutScroll( m_state->offset );
		m_scrollArea.drawWidgetColor( UiColor::Black );

		ImUiWidgetInputState widgetInput;
		m_scrollArea.getInputState( widgetInput );

		const UiInputState& inputState = m_scrollArea.getContext().getInput();
		if( widgetInput.wasPressed )
		{
			const UiPos mousePos = inputState.getMousePos();
			if( widgetInput.hasMousePressed )
			{
				m_state->lastMousePos = mousePos;
			}

			const UiPos deltaPos = mousePos - m_state->lastMousePos;
			m_state->lastMousePos = mousePos;

			m_state->offset -= deltaPos;
		}

		if( widgetInput.isMouseOver )
		{
			m_state->zoom += max( 0.05f, roundf( m_state->zoom ) * 0.1f ) * inputState.getMouseScrollDelta().y;
			m_state->zoom = max( 0.05f, m_state->zoom );
		}
	}

	ResourceTool::ImageViewWidget::~ImageViewWidget()
	{
		m_scrollContent.endWidget();
		m_scrollArea.endWidget();

		UiToolboxWindow window = getWindow();

		UiWidgetLayoutHorizontal buttonsLayout( window );

		//if( window.buttonIcon( ImAppImageGetTexture( m_icon ) ) )
		//{
		//
		//}

		//if( window.buttonIcon( ImAppImageGetTexture( m_icon ) ) )
		//{
		//
		//}

		window.labelFormat( "Zoom: %.0f %%", m_state->zoom * 100.0f );
	}

}
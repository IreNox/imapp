#include "resource_tool.h"

#include "resource.h"
#include "resource_helpers.h"

#include <imapp/imapp.h>
#include <imui/imui_cpp.h>

#include <tiki/tiki_path.h>

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
		if( imapp->dropData )
		{
			handleDrop( imapp->dropData );
		}

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
				UiWidgetLayoutVertical viewWidget( window, 4.0f );
				viewWidget.setStretch( UiSize::One );

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
		size_t selectedIndex = (size_t)-1;
		DynamicArray< UiStringView > imageResourceNames;
		{
			imageResourceNames.reserve( m_package.getResourceCount() );

			for( uintsize i = 0; i < m_package.getResourceCount(); ++i )
			{
				const Resource& imageResource = m_package.getResource( i );
				if( imageResource.getType() != ResourceType::Image )
				{
					continue;
				}

				if( imageResource.getName() == resource.getSkinImageName() )
				{
					selectedIndex = i;
				}

				imageResourceNames.pushBack( (RtStr)imageResource.getName() );
			}
		}

		window.label( "Image:" );
		{
			UiToolboxDropdown imageSelect( window, imageResourceNames.getData(), imageResourceNames.getLength() );
			imageSelect.setStretch( UiSize::Horizontal );

			const uintsize newSelectedIndex = imageSelect.getSelectedIndex();
			if( newSelectedIndex < imageResourceNames.getLength() )
			{
				if( newSelectedIndex != selectedIndex )
				{
					const StringView selectedName = (RtStr)imageResourceNames[ newSelectedIndex ];
					resource.setSkinImageName( selectedName );
				}
			}
			else
			{
				imageSelect.setSelectedIndex( selectedIndex );
			}
		}

		Resource* imageResource = m_package.findResource( resource.getSkinImageName() );
		if( imageResource )
		{
			const ImUiTexture imageTexture = ImAppImageGetTexture( imageResource->getImage() );

			UiBorder& skinBorder = resource.getSkinBorder();
			{
				UiWidgetLayoutHorizontal skinLayout( window, 4.0f );
				skinLayout.setStretch( UiSize::Horizontal );

				struct BorderInfo { UiStringView title; float& value; } borders[] =
				{
					{ "Top:", skinBorder.top },
					{ "Left:", skinBorder.left },
					{ "Right:", skinBorder.right },
					{ "Bottom:", skinBorder.bottom }
				};

				for( size_t i = 0; i < 4u; ++i )
				{
					BorderInfo& border = borders[ i ];

					UiWidgetLayoutVertical borderLayout( window, 4.0f );
					borderLayout.setStretch( UiSize::Horizontal );

					window.label( border.title );
					const bool changed = window.slider( border.value, 0.0f, imageTexture.size.height );

					bool isNew;
					char* buffer = (char*)borderLayout.allocState( 128u, isNew );
					if( changed || isNew )
					{
						snprintf( buffer, 128u, "%.0f", border.value );
					}

					if( window.textEdit( buffer, 128u ) )
					{
						border.value = (float)atof( buffer );
					}
				}
			}

			const bool previewSkin = window.checkBoxState( "Preview Skin" );

			ImageViewWidget imageView( window );

			UiWidget image( window );
			image.setFixedSize( (UiSize)imageTexture.size * imageView.getZoom() );

			if( previewSkin )
			{
				ImUiSkin skin;
				skin.texture	= imageTexture;
				skin.border		= skinBorder;
				skin.uv			= UiTexCoord::ZeroToOne;

				image.drawWidgetSkin( skin );
			}
			else
			{
				image.drawWidgetTexture( imageTexture );

				UiBorder drawBorder = skinBorder;
				drawBorder.top *= imageView.getZoom();
				drawBorder.left *= imageView.getZoom();
				drawBorder.right *= imageView.getZoom();
				drawBorder.bottom *= imageView.getZoom();

				const UiRect imageRect = image.getRect();
				const UiRect innerRect = imageRect.shrinkBorder( drawBorder );
				const UiColor color = UiColor( (uint8)0x30u, 0x90u, 0xe0u );
				image.drawLine( UiPos( imageRect.pos.x, innerRect.pos.y ), UiPos( imageRect.getRight(), innerRect.pos.y ), color );
				image.drawLine( UiPos( innerRect.pos.x, imageRect.pos.y ), UiPos( innerRect.pos.x, imageRect.getBottom() ), color );
				image.drawLine( UiPos( innerRect.getRight(), imageRect.pos.y ), UiPos( innerRect.getRight(), imageRect.getBottom() ), color );
				image.drawLine( UiPos( imageRect.pos.x, innerRect.getBottom() ), UiPos( imageRect.getRight(), innerRect.getBottom() ), color );
			}
		}
		else
		{
			window.labelFormat( "No Image selected!" );
		}
	}

	void ResourceTool::doViewConfig( UiToolboxWindow& window, Resource& resource )
	{

	}

	void ResourceTool::handleDrop( const char* dropData )
	{
		const Path packageDir = Path( m_package.getPath() ).getParent();

		Path path( dropData );
		if( !path.getGenericPath().startsWithNoCase( packageDir.getGenericPath() ) )
		{
			showError( "File not in same tree as package." );
			return;
		}

		const DynamicString ext = path.getExtension();
		if( ext != ".png" )
		{
			showError( "Not supported file format: %s", ext.getData() );
			return;
		}

		const DynamicString remainingPath = path.getGenericPath().subString( packageDir.getGenericPath().getLength() + 1u );
		const DynamicString filename = path.getBasename();

		Resource& resource = m_package.addResource( filename, ResourceType::Image );
		resource.setImageSourcePath( remainingPath );
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

		bool isNew;
		m_state = newState< ScrollState >( isNew );

		m_scrollArea.setStretch( UiSize::One );
		m_scrollArea.setLayoutScroll( m_state->offset );
		m_scrollArea.drawWidgetColor( UiColor::Black );

		ImUiWidgetInputState widgetInput;
		m_scrollArea.getInputState( widgetInput );

		m_scrollContent.setAlign( UiAlign::Center );

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
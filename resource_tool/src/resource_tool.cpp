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
		doState( surface );

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
				}

				{
					UiWidgetLayoutHorizontal buttonsLayout( window, 8.0f );
					buttonsLayout.setStretch( UiSize::Horizontal );

					UiToolboxButtonLabel plusButton( window, "+" );
					plusButton.setStretch( UiSize::Horizontal );
					if( plusButton.end() )
					{
						m_state = State::New;
					}

					UiToolboxButtonLabel minusButton( window, "-" );
					minusButton.setStretch( UiSize::Horizontal );
					if( minusButton.end() )
					{
						m_state = State::DeleteConfirm;
					}
				}
			}

			{
				UiWidget viewWidget( window );
				viewWidget.setStretch( UiSize::One );

				const ImUiTexture image = ImAppImageGetBlocking( imapp, IMUI_STR( "test.png" ) );
				viewWidget.drawWidgetTexture( image );
			}
		}

	}

	void ResourceTool::doState( UiSurface& surface )
	{
		switch( m_state )
		{
		case ResourceTool::State::Home:
			break;

		case ResourceTool::State::New:
			doStateNew( surface );
			break;

		case ResourceTool::State::DeleteConfirm:
			doStateDeleteConfirm( surface );
			break;

		case ResourceTool::State::Error:
			doStateError( surface );
			break;
		}
	}

	void ResourceTool::doStateNew( UiSurface& surface )
	{
		UiToolboxPopup popup( surface );

		UiStringView name;
		UiStringView path;
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

			popup.label( "Path:" );
			path = popup.textEditState( 128u );
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
		if( path.isEmpty() )
		{
			popup.label( "No path set!" );
			ok = false;
		}

		const UiStringView buttons[] = { "Ok", "Cancel" };
		const size_t buttonIndex = popup.end( buttons, TIKI_ARRAY_COUNT( buttons ) );
		if( buttonIndex == 0u && ok )
		{
			m_package.addResource( (RtStr)name, (ResourceType)typeIndex, (RtStr)path );
			m_state = State::Home;
		}
		else if( buttonIndex == 1u )
		{
			m_state = State::Home;
		}
	}

	void ResourceTool::doStateDeleteConfirm( UiSurface& surface )
	{

	}

	void ResourceTool::doStateError( UiSurface& surface )
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
			m_state = State::Home;
		}
	}

	void ResourceTool::showError( const char* format, ... )
	{
		va_list args;
		va_start( args, format );
		m_errorMessage = DynamicString::formatArgs( format, args );
		va_end( args );

		m_state = State::Error;
	}
}
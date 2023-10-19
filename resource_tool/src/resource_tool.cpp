#include "resource_tool.h"

#include "resource_helpers.h"
#include "resource_theme.h"

#include "file_dialog.h"

#include <imapp/imapp.h>
#include <imui/imui_cpp.h>

#include <tiki/tiki_path.h>
#include <tiki/tiki_string_tools.h>

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
			showError( "Failed to load package '%s'.", filename );
		}

		updateResourceNamesByType();
	}

	void ResourceTool::doUi( ImAppContext* imapp, UiSurface& surface )
	{
		{
			ImAppDropData dropData;
			while( ImAppWindowPopDropData( imapp->defaultWindow, &dropData ) )
			{
				if( dropData.type == ImAppDropType_Text )
				{
					continue;
				}

				handleDrop( dropData.pathOrText );
			}
		}

		m_package.updateFileData( imapp, surface.getTime() );

		doPopupState( surface );

		UiToolboxWindow window( surface, "main", surface.getRect(), 1 );

		UiWidgetLayoutVertical mainLayout( window, 8.0f );
		mainLayout.setStretch( UiSize::One );
		mainLayout.setMargin( UiBorder( 8.0f ) );

		{
			UiWidgetLayoutHorizontal titleLayout( window, 8.0f );
			titleLayout.setStretchHorizontal();

			if( window.buttonLabel( "Open" ) )
			{
				const ArrayView< StringView > filters = { "I'm App Resource Package(*.iaresx)", "*.iaresx" };
				const DynamicString filePath = openFileDialog( "Open package...", "", filters );
				if( filePath.hasElements() )
				{
					load( filePath );
				}
			}

			if( window.buttonLabel( "Save" ) )
			{
				if( m_package.hasPath() )
				{
					if( !m_package.save() )
					{
						showError( "Failed to save package." );
					}
				}
				else
				{
					const ArrayView< StringView > filters ={ "I'm App Resource Package(*.iaresx)", "*.iaresx" };
					const DynamicString filePath = saveFileDialog( "Save package as...", "", filters );
					if( filePath.hasElements() &&
						!m_package.saveAs( filePath ) )
					{
						showError( "Failed to save package: %s", filePath.toConstCharPointer() );
					}
				}
			}

			window.label( (RtStr)m_package.getName() );

			window.strecher( 1.0f, 0.0f );

			window.progressBar( -1.0f );

			if( m_package.getOutputPath().hasElements() )
			{
				window.checkBoxState( "Auto Compile" );

				if( m_compiler.isRunning() )
				{
					window.progressBar( -1.0f );
					window.label( "Compiling..." );
				}
				else if( window.buttonLabel( "Compile" ) )
				{
					const Path outputPath = m_package.getPath().getParent().push( m_package.getOutputPath() );
					m_compiler.startCompile( outputPath.getGenericPath(), m_package );
				}
			}
		}

		{
			UiWidgetLayoutHorizontal workLayout( window, 8.0f );
			workLayout.setStretch( UiSize::One );

			{
				UiWidgetLayoutVertical leftLayout( window );
				leftLayout.setStretch( UiSize::Vertical );

				{
					UiToolboxList list( window, 20.0f, m_package.getResourceCount() + 1u );
					list.setStretch( UiSize::One );
					list.setMinWidth( 150.0f );

					list.setSelectedIndex( m_selecedEntry );

					list.nextItem();
					window.label( "Package" );

					for( uintsize i = 0; i < m_package.getResourceCount(); ++i )
					{
						const Resource& resource = m_package.getResource( i );

						list.nextItem();

						window.label( (RtStr)resource.getName() );
					}

					m_selecedEntry = list.getSelectedIndex();
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

			doView( window );
		}

		if( m_compiler.getOutputs().hasElements() )
		{
			UiToolboxList outputList( window, 25.0f, m_compiler.getOutputs().getLength() );
			outputList.setStretch( UiSize( 1.0f, 0.0f ) );
			outputList.setFixedHeight( 150.0f );

			for( const ResourceCompilerOutput& output : m_compiler.getOutputs() )
			{
				outputList.nextItem();

				window.label( (RtStr)output.message );
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
			// HACK: really ugly but works. need a way to convert to UiStringView in a proper way.
			{
				UiToolboxDropdown dropDown( popup, (const UiStringView*)items.getData(), items.getLength() );
				dropDown.setStretch( UiSize::Horizontal );

				typeIndex = dropDown.getSelectedIndex();
			}

			popup.spacer( 1.0f, 4.0f );
		}

		bool ok = true;
		if( name.isEmpty() )
		{
			popup.label( "No name set!" );
			ok = false;
		}
		if( m_package.findResource( (RtStr)name ) )
		{
			popup.label( "No name must be unique!" );
			ok = false;
		}
		if( typeIndex >= (uintsize)ResourceType::Count )
		{
			popup.label( "No type set!" );
			ok = false;
		}

		const UiStringView buttons[] = { "Ok", "Cancel" };
		const size_t buttonIndex = popup.end( buttons, TIKI_ARRAY_COUNT( buttons ) );
		if( buttonIndex == 0u && ok )
		{
			const ResourceType type = (ResourceType)typeIndex;
			m_package.addResource( (RtStr)name, type );
			updateResourceNamesByType();

			m_popupState = PopupState::Home;
		}
		else if( buttonIndex == 1u )
		{
			m_popupState = PopupState::Home;
		}
	}

	void ResourceTool::doPopupStateDeleteConfirm( UiSurface& surface )
	{
		UiToolboxPopup popup( surface );

		const uintsize selectedResource = m_selecedEntry - 1u;
		if( selectedResource >= m_package.getResourceCount() )
		{
			m_popupState = PopupState::Home;
			return;
		}

		const Resource& resource = m_package.getResource( selectedResource );
		popup.labelFormat( "Do you really want to delete '%s'?", resource.getName().toConstCharPointer() );

		popup.spacer( 1.0f, 8.0f );

		const UiStringView buttons[] = { "Yes", "No" };
		const size_t buttonIndex = popup.end( buttons, TIKI_ARRAY_COUNT( buttons ) );
		if( buttonIndex < TIKI_ARRAY_COUNT( buttons ) )
		{
			if( buttonIndex == 0u )
			{
				m_package.removeResource( selectedResource );
				m_selecedEntry--;
			}
			m_popupState = PopupState::Home;
		}
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
		UiWidget viewWidget( window, (ImUiId)m_selecedEntry + 564646u );
		viewWidget.setStretch( UiSize::One );
		viewWidget.setLayoutVerticalSpacing( 4.0f );

		if( m_selecedEntry == 0u )
		{
			doViewPackage( window );
			return;
		}

		const uintsize selectedResource = m_selecedEntry - 1u;
		if( selectedResource >= m_package.getResourceCount() )
		{
			window.label( "No resource selected." );
			return;
		}

		Resource& resource = m_package.getResource( selectedResource );

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

		case ResourceType::Theme:
			doViewConfig( window, resource );
			break;
		}
	}

	void ResourceTool::doViewPackage( UiToolboxWindow& window )
	{
		window.label( "Name:" );
		{
			UiWidgetLayoutHorizontal pathLayout( window, 8.0f );
			pathLayout.setStretch( UiSize::Horizontal );

			const RtStr newName = window.textEditState( 256u, (RtStr)m_package.getName() );
			if( newName != m_package.getName() &&
				window.buttonLabel( "Change" ) )
			{
				m_package.setName( newName );
			}
		}

		window.label( "Output Path:" );
		{
			UiWidgetLayoutHorizontal pathLayout( window, 8.0f );
			pathLayout.setStretch( UiSize::Horizontal );

			const RtStr newPath = window.textEditState( 256u, (RtStr)m_package.getOutputPath() );
			if( newPath != m_package.getOutputPath() &&
				window.buttonLabel( "Change" ) )
			{
				m_package.setOutputPath( newPath );
			}
		}
	}

	void ResourceTool::doViewImage( UiToolboxWindow& window, Resource& resource )
	{
		window.label( "Path:" );
		{
			UiWidgetLayoutHorizontal pathLayout( window, 8.0f );
			pathLayout.setStretch( UiSize::Horizontal );

			const RtStr newPath = window.textEditState( 256u, (RtStr)resource.getFileSourcePath() );
			if( newPath != resource.getFileSourcePath() &&
				window.buttonLabel( "Change" ) )
			{
				resource.setFileSourcePath( newPath );
			}
		}

		bool allowAtlas = resource.getImageAllowAtlas();
		window.checkBox( allowAtlas, "Allow Atlas" );
		resource.setImageAllowAtlas( allowAtlas );

		const ImUiTexture imageTexture = ImAppImageGetImage( resource.getImage() );

		{
			ImageViewWidget imageView( window );

			UiWidget image( window );
			image.setFixedSize( (UiSize)imageTexture * imageView.getZoom() );

			image.drawWidgetTexture( imageTexture );
		}

		if( window.buttonLabel( "Create Skin" ) )
		{
			DynamicString name = resource.getName();
			name = "skin_" + name;

			Resource& skinResource = m_package.addResource( name, ResourceType::Skin );
			updateResourceNamesByType();

			skinResource.setSkinImageName( resource.getName() );

			m_selecedEntry = m_package.getResourceCount() - 1u;
		}
	}

	void ResourceTool::doViewSkin( UiToolboxWindow& window, Resource& resource )
	{
		window.label( "Image:" );
		const StringView selectedImageName = doResourceSelect( window, ResourceType::Image, resource.getSkinImageName() );
		if( selectedImageName != resource.getSkinImageName() )
		{
			resource.setSkinImageName( selectedImageName );
		}

		Resource* imageResource = m_package.findResource( resource.getSkinImageName() );
		if( imageResource )
		{
			const ImUiTexture imageTexture = ImAppImageGetImage( imageResource->getImage() );

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

					if( window.slider( border.value, 0.0f, (float)imageTexture.height ) )
					{
						border.value = floorf( border.value );
						resource.increaseRevision();
					}

					if( doFloatTextEdit( window, border.value ) )
					{
						resource.increaseRevision();
					}
				}
			}

			const bool previewSkin = window.checkBoxState( "Preview Skin" );

			ImageViewWidget imageView( window );

			UiWidget image( window );
			image.setFixedSize( (UiSize)imageTexture * imageView.getZoom() );

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
		UiToolboxScrollArea scrollArea( window );
		scrollArea.setStretch( UiSize::One );
		scrollArea.enableSpacing( true, false );

		UiWidgetLayoutVertical scrollLayout( window );
		scrollLayout.setStretch( UiSize::Horizontal );
		scrollLayout.setLayoutVertical();

		bool skipGroup = false;
		for( ResourceThemeField& field : resource.getTheme().getFields() )
		{
			if( field.type == ResourceThemeFieldType::Group )
			{
				skipGroup = !window.checkBoxState( (RtStr)field.name, true );
			}
			else if( skipGroup )
			{
				continue;
			}
			else
			{
				window.label( (RtStr)field.name );
			}

			switch( field.type )
			{
			case ResourceThemeFieldType::Group:
				break;

			case ResourceThemeFieldType::Font:
				{
					const StringView fontName = doResourceSelect( window, ResourceType::Font, *field.data.fontNamePtr );
					if( *field.data.fontNamePtr != fontName )
					{
						*field.data.fontNamePtr = fontName;
						resource.increaseRevision();
					}
				}
				break;

			case ResourceThemeFieldType::Color:
				{
					UiWidgetLayoutHorizontal colorLayout( window, 4.0f );
					colorLayout.setStretch( UiSize::Horizontal );

					ImUiColor& value = *field.data.colorPtr;

					{
						UiWidget previewWidget( window );
						previewWidget.setStretch( UiSize::Vertical );
						previewWidget.setFixedWidth( 50.0f );

						previewWidget.drawWidgetColor( UiColor::Black );
						previewWidget.drawWidgetColor( (UiColor)value );
					}

					struct ColorEditState
					{
						uintsize					length;
						StaticArray< char, 32u >	buffer;
					};

					UiToolboxTextEdit textEdit( window );

					bool isNew;
					ColorEditState* state = textEdit.newState< ColorEditState >( isNew );
					if( isNew )
					{
						state->length = formatUiColor( state->buffer.getData(), state->buffer.getLength(), value );
					}

					textEdit.setBuffer( state->buffer.getData(), state->buffer.getLength() );

					if( textEdit.end( &state->length ) )
					{
						parseUiColor( value, StringView( state->buffer.getData(), state->length ) );
						resource.increaseRevision();
					}
				}
				break;

			case ResourceThemeFieldType::Skin:
				{
					UiWidgetLayoutHorizontal skinLayout( window, 4.0f );
					skinLayout.setStretch( UiSize::Horizontal );

					{
						UiWidget previewWidget( window );
						previewWidget.setStretch( UiSize::Vertical );
						previewWidget.setFixedWidth( 50.0f );

						Resource* skinResource = m_package.findResource( *field.data.skinNamePtr );
						Resource* imageResource = nullptr;
						if( skinResource )
						{
							imageResource = m_package.findResource( skinResource->getSkinImageName() );
						}

						if( skinResource && imageResource )
						{
							const ImUiTexture imageTexture = ImAppImageGetImage( imageResource->getImage() );

							ImUiSkin skin;
							skin.texture	= imageTexture;
							skin.border		= skinResource->getSkinBorder();
							skin.uv			= UiTexCoord::ZeroToOne;
							previewWidget.drawWidgetSkin( skin );
						}
						else
						{
							previewWidget.drawWidgetColor( UiColor( (uint8)0xffu, 0u, 0u ) );
						}
					}

					const StringView skinName = doResourceSelect( window, ResourceType::Skin, *field.data.skinNamePtr );
					if( *field.data.skinNamePtr != skinName )
					{
						*field.data.skinNamePtr = skinName;
						resource.increaseRevision();
					}
				}
				break;

			case ResourceThemeFieldType::Float:
				if( doFloatTextEdit( window, *field.data.floatPtr ) )
				{
					resource.increaseRevision();
				}
				break;

			case ResourceThemeFieldType::Border:
				if( doFloatTextEdit( window, field.data.borderPtr->top ) ||
					doFloatTextEdit( window, field.data.borderPtr->left ) ||
					doFloatTextEdit( window, field.data.borderPtr->right ) ||
					doFloatTextEdit( window, field.data.borderPtr->bottom ) )
				{
					resource.increaseRevision();
				}
				break;

			case ResourceThemeFieldType::Size:
				if( doFloatTextEdit( window, field.data.sizePtr->width ) ||
					doFloatTextEdit( window, field.data.sizePtr->height ) )
				{
					resource.increaseRevision();
				}
				break;

			case ResourceThemeFieldType::Image:
				{
					UiWidgetLayoutHorizontal imageLayout( window, 4.0f );
					imageLayout.setStretch( UiSize::Horizontal );

					{
						UiWidget previewWidget( window );
						previewWidget.setStretch( UiSize::Vertical );
						previewWidget.setFixedWidth( 50.0f );

						Resource* imageResource = m_package.findResource( *field.data.imageNamePtr );
						if( imageResource )
						{
							const ImUiTexture imageTexture = ImAppImageGetImage( imageResource->getImage() );

							const float width = (previewWidget.getRect().size.height / float( imageTexture.height )) * float( imageTexture.width );
							previewWidget.setFixedWidth( width );

							previewWidget.drawWidgetTexture( imageTexture );
						}
						else
						{
							previewWidget.drawWidgetColor( UiColor( (uint8)0xffu, 0u, 0u ) );
						}
					}

					const StringView imageName = doResourceSelect( window, ResourceType::Image, *field.data.imageNamePtr );
					if( *field.data.imageNamePtr != imageName )
					{
						*field.data.imageNamePtr = imageName;
						resource.increaseRevision();
					}
				}
				break;

			case ResourceThemeFieldType::UInt32:
				break;
			}
		}
	}

	bool ResourceTool::doFloatTextEdit( UiToolboxWindow& window, float& value )
	{
		struct FloatEditState
		{
			float						lastValue;
			StaticArray< char, 32u >	buffer;
		};

		UiToolboxTextEdit textEdit( window );

		bool isNew;
		FloatEditState* state = textEdit.newState< FloatEditState >( isNew );
		if( isNew || state->lastValue != value )
		{
			snprintf( state->buffer.getData(), state->buffer.getLength(), "%.0f", value );
		}

		textEdit.setBuffer( state->buffer.getData(), state->buffer.getLength() );

		if( textEdit.end() )
		{
			string_tools::tryParseFloat( value, StringView( state->buffer.getData() ) );
			state->lastValue = value;
			return true;
		}

		return false;
	}

	StringView ResourceTool::doResourceSelect( UiToolboxWindow& window, ResourceType type, const StringView& selectedResourceName )
	{
		uintsize selectedIndex = -1;
		const UiStringView selectedResourceNameUi = (RtStr)selectedResourceName;
		const ArrayView< UiStringView > resourceNames = m_resourceNamesByType[ (uintsize)type ];
		for( uintsize i = 0u; i < resourceNames.getLength(); ++i )
		{
			const UiStringView& resourceName = resourceNames[ i ];
			if( resourceName == selectedResourceNameUi )
			{
				selectedIndex = i;
				break;
			}
		}

		UiToolboxDropdown resourceSelect( window, resourceNames.getData(), resourceNames.getLength() );
		resourceSelect.setStretch( UiSize::Horizontal );

		const uintsize newSelectedIndex = resourceSelect.getSelectedIndex();
		if( newSelectedIndex < resourceNames.getLength() )
		{
			return (RtStr)resourceNames[ newSelectedIndex ];
		}
		else
		{
			resourceSelect.setSelectedIndex( selectedIndex );
		}

		return StringView();
	}

	void ResourceTool::handleDrop( const char* dropData )
	{
		const Path packageDir = m_package.getPath().getParent();

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

		if( m_package.findResource( filename ) )
		{
			showError( "Can't add '%s' because there is already a resource with this name.", filename.toConstCharPointer() );
			return;
		}

		Resource& resource = m_package.addResource( filename, ResourceType::Image );
		updateResourceNamesByType();

		resource.setFileSourcePath( remainingPath );

		m_selecedEntry = m_package.getResourceCount();
	}

	void ResourceTool::showError( const char* format, ... )
	{
		va_list args;
		va_start( args, format );
		m_errorMessage = DynamicString::formatArgs( format, args );
		va_end( args );

		m_popupState = PopupState::Error;
	}

	void ResourceTool::updateResourceNamesByType()
	{
		for( DynamicArray< UiStringView >& array : m_resourceNamesByType )
		{
			array.clear();
		}

		for( const Resource& resource : m_package.getResources() )
		{
			m_resourceNamesByType[ (uintsize)resource.getType() ].pushBack( (RtStr)resource.getName() );
		}
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
		//m_scrollArea.drawWidgetColor( UiColor::Black );

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
			getContext().setMouseCursor( ImUiInputMouseCursor_Move );

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
		buttonsLayout.setPadding( UiBorder( 4.0f ) );

		buttonsLayout.drawWidgetColor( UiColor::CreateBlack( 0x80u ) );

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
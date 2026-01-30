#include "resource_tool.h"

#include "resource_helpers.h"
#include "resource_theme.h"
#include "resource_unicode.h"

#include "file_dialog.h"

#include <imapp/imapp.h>
#include <imui/imui_cpp.h>

#include <tiki/tiki_path.h>
#include <tiki/tiki_string.h>
#include <tiki/tiki_string_tools.h>

#include <cstdarg>

namespace imapp
{
	// https://www.realtimecolors.com
	//--text: #0d0e1c;
	//--background: #f7f7f7;
	//--primary: #2f356a;
	//--secondary: #d8daee;
	//--accent: #4d57ac;
	// https://coolors.co
	// black olive: 414535
	// dark purple: 290628
	// https://www.canva.com
	// Royal Blue
	// #0087D6
	// #0081E6
	// #0077E9
	// #4169E1
	// #8A57CE
	// #B342B2
	// #CC2D90


	using namespace imui;
	using namespace imui::toolbox;

	ResourceTool::ResourceTool()
	{
	}

	bool ResourceTool::handleArgs( int argc, char* argv[], bool& shutdown )
	{
		int argi = 1;
		bool compile = false;
		for( ; argi < argc - 1; ++argi )
		{
			const char* arg = argv[ argi ];

			if( isStringEquals( arg, "-c" ) ||
				isStringEquals( arg, "--compile" ) )
			{
				compile = true;
			}
			else if( isStringEquals( arg, "--help" ) )
			{
				const char* helpFormat = R"V0G0N(Using %s [OPTIONS] filename

Options:
-c, --compile           Compile loaded resource package
-e, --export-c output   Writes resource package as C file to 'output'
--help                  Show this help message
)V0G0N";

				Path exePath( argv[ 0u ] );
				ImAppTrace( helpFormat, exePath.getFilename().toConstCharPointer() );
				shutdown = true;
				return true;
			}
		}

		bool loaded = false;
		const char* filename = nullptr;
		if( argi < argc )
		{
			filename = argv[ argi ];
			loaded = load( filename );
		}

		if( compile && !loaded )
		{
			ImAppTrace( "Error: Failed to load '%s'\n", filename ? filename : "no file" );
			return false;
		}

		if( compile )
		{
			m_package.updateFileData( 0.0 );

			m_compiler.startCompile( m_package );
			m_compiler.waitForCompile();

			const CompilerOutput& output = m_compiler.getOutput();
			for( const CompilerMessage& message : output.getMessages() )
			{
				const char* level = "Error";
				switch( message.level )
				{
				case CompilerErrorLevel::Info:		level = "Info"; break;
				case CompilerErrorLevel::Warning:	level = "Warning"; break;
				case CompilerErrorLevel::Error:		break;
				}
				ImAppTrace( "%s: [%s] %s\n", level, message.resourceName.toConstCharPointer(), message.text.toConstCharPointer() );
			}

			if( output.hasError() )
			{
				return false;
			}

			shutdown = true;
		}

		return true;
	}

	void ResourceTool::doUi( ImAppContext* imapp, ImAppWindow* appWindow, UiToolboxWindow& uiWindow )
	{
		update( appWindow, uiWindow.getTime() );

		doPopups( uiWindow.getSurface() );
		doNotifications( uiWindow.getSurface() );

		UiWidgetLayoutVertical mainLayout( uiWindow, 8.0f );
		mainLayout.setStretchOne();
		mainLayout.setMargin( UiBorder( 8.0f ) );

		{
			UiWidgetLayoutHorizontal titleLayout( uiWindow, 8.0f );
			titleLayout.setHStretch( 1.0f );

			if( uiWindow.buttonLabel( "Open" ) )
			{
				const StringView filters[] = { "I'm App Resource Package(*.iaresx)", "*.iaresx" };
				const DynamicString filePath = openFileDialog( "Open package...", "", ConstArrayView< StringView >( filters, TIKI_ARRAY_COUNT( filters ) ) );
				if( filePath.hasElements() )
				{
					load( filePath );
				}
			}

			if( uiWindow.buttonLabel( "Save" ) )
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
					const StringView filters[] = { "I'm App Resource Package(*.iaresx)", "*.iaresx" };
					const DynamicString filePath = saveFileDialog( "Save package as...", "", ConstArrayView< StringView >( filters, TIKI_ARRAY_COUNT( filters ) ) );
					if( filePath.hasElements() &&
						!m_package.saveAs( filePath ) )
					{
						showError( "Failed to save package: %s", filePath.toConstCharPointer() );
					}
				}
				m_notifications.set( Notifications::Saved );
			}

			uiWindow.label( m_package.getName() );

			uiWindow.strecher( 1.0f, 0.0f );

			//window.progressBar( -1.0f );

			if( m_package.getOutputPath().hasElements() )
			{
				uiWindow.checkBox( m_autoCompile, "Auto Compile" );

				const bool isCompiling = m_compiler.isRunning();
				if( isCompiling )
				{
					uiWindow.progressBar( -1.0f );
					uiWindow.label( "Compiling..." );
					m_wasCompiling = true;
				}
				else if( uiWindow.buttonLabel( "Compile" ) )
				{
					m_compiler.startCompile( m_package );
					m_wasCompiling = true;
				}

				if( !isCompiling && m_wasCompiling )
				{
					m_notifications.set( Notifications::Compiled );
					m_wasCompiling = false;
				}
			}
		}

		{
			UiWidgetLayoutHorizontal workLayout( uiWindow, 8.0f );
			workLayout.setStretchOne();

			{
				UiWidget leftLayout( uiWindow, "left" );
				leftLayout.setLayoutVertical( 4.0f );
				leftLayout.setVStretch( 1.0f );

				{
					UiToolboxList list( uiWindow, 22.0f, m_package.getResourceCount() + 1u, true );
					list.setStretchOne();
					list.setMinWidth( 200.0f );

					if( m_overrideSelectedEntry )
					{
						list.setSelectedIndex( m_selecedEntry );
						m_overrideSelectedEntry = false;
					}

					uintsize startIndex = list.getBeginIndex();
					if( startIndex == 0u )
					{
						UiWidget item;
						list.nextItem( &item );
						item.setPadding( UiBorder( 0.0f, 4.0f, 0.0f, 0.0f ) );

						uiWindow.label( "Package" );

						startIndex++;
					}

					for( uintsize i = startIndex; i < list.getEndIndex(); ++i )
					{
						const Resource& resource = m_package.getResource( i - 1u );

						UiWidget item;
						list.nextItem( &item );
						item.setPadding( UiBorder( 0.0f, 4.0f, 0.0f, 0.0f ) );

						uiWindow.label( resource.getName() );
					}

					m_selecedEntry = list.getSelectedIndex();
				}

				{
					UiWidgetLayoutHorizontal buttonsLayout( uiWindow, 8.0f );
					buttonsLayout.setHStretch( 1.0f );

					UiToolboxButtonLabel plusButton( uiWindow, "+" );
					plusButton.setHStretch( 1.0f );
					if( plusButton.end() )
					{
						m_popupState = PopupState::New;
					}

					UiToolboxButtonLabel minusButton( uiWindow, "-" );
					minusButton.setHStretch( 1.0f );
					if( minusButton.end() )
					{
						m_popupState = PopupState::DeleteConfirm;
					}
				}
			}

			doView( imapp, uiWindow );
		}

		if( m_compiler.getOutput().getMessages().hasElements() )
		{
			UiToolboxList outputList( uiWindow, 25.0f, m_compiler.getOutput().getMessages().getLength(), false );
			outputList.setHStretch( 1.0f );
			outputList.setFixedHeight( 150.0f );

			for( const CompilerMessage& message : m_compiler.getOutput().getMessages() )
			{
				outputList.nextItem();

				uiWindow.label( message.text );
			}
		}
	}

	bool ResourceTool::load( const char* filename )
	{
		const bool loaded = m_package.load( (StringView)filename );
		if( !loaded )
		{
			showError( "Failed to load package '%s'.", filename );
		}

		m_compiler.reset();
		updateResourceNamesByType();
		m_notifications.set( Notifications::Loaded );

		return loaded;
	}

	void ResourceTool::update( ImAppWindow* appWindow, double time )
	{
		m_package.updateFileData( time );

		{
			const uint32 revision = m_package.getRevision();

			if( m_autoCompile &&
				!m_compiler.isRunning() &&
				m_lastCompileRevision != revision )
			{
				m_compiler.startCompile( m_package );
				m_lastCompileRevision = revision;
			}
		}

		{
			ImAppDropData dropData;
			while( ImAppWindowPopDropData( appWindow, &dropData ) )
			{
				if( dropData.type == ImAppDropType_Text )
				{
					continue;
				}

				handleDrop( dropData.pathOrText );
			}
		}
	}

	void ResourceTool::doPopups( UiSurface& surface )
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

		const char* name;
		size_t typeIndex;
		{
			UiWidgetLayoutVertical layout( popup );
			layout.setMinWidth( 250.0f );

			popup.label( "Name:" );
			name = popup.textEditState( 64u );
			popup.spacer( 1.0f, 4.0f );

			popup.label( "Type:" );
			const ArrayView< const char* > items = getResourceTypeStrings();
			{
				UiToolboxDropdown dropDown( popup, (const char**)items.getData(), items.getLength() );
				dropDown.setHStretch( 1.0f );

				typeIndex = dropDown.getSelectedIndex();
			}

			popup.spacer( 1.0f, 4.0f );
		}

		bool ok = true;
		if( !name || !*name )
		{
			popup.label( "No name set!" );
			ok = false;
		}
		if( m_package.findResource( (StringView)name ) )
		{
			popup.label( "No name must be unique!" );
			ok = false;
		}
		if( typeIndex >= (uintsize)ResourceType::Count )
		{
			popup.label( "No type set!" );
			ok = false;
		}

		const char* buttons[] = { "Ok", "Cancel" };
		const size_t buttonIndex = popup.end( buttons, TIKI_ARRAY_COUNT( buttons ) );
		if( buttonIndex == 0u && ok )
		{
			const ResourceType type = (ResourceType)typeIndex;
			m_package.addResource( (StringView)name, type );
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

		const char* buttons[] = { "Yes", "No" };
		const size_t buttonIndex = popup.end( buttons, TIKI_ARRAY_COUNT( buttons ) );
		if( buttonIndex < TIKI_ARRAY_COUNT( buttons ) )
		{
			if( buttonIndex == 0u )
			{
				m_package.removeResource( selectedResource );
				m_selecedEntry--;
				m_overrideSelectedEntry = true;
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

			popup.label( m_errorMessage );
		}

		const char* buttons[] = { "Ok" };
		const size_t buttonIndex = popup.end( buttons, TIKI_ARRAY_COUNT( buttons ) );
		if( buttonIndex == 0u )
		{
			m_popupState = PopupState::Home;
		}
	}

	void ResourceTool::doNotifications( UiSurface& surface )
	{
		if( m_notifications.isEmpty() )
		{
			return;
		}

		const float count = (float)m_notifications.getCount();

		UiRect notificationsRect;
		notificationsRect.size.width	= 200.0f;
		notificationsRect.size.height	= (50.0f * count) + (10.0f * (count - 1.0f));
		notificationsRect.pos.x			= surface.getSize().width - 208.0f;
		notificationsRect.pos.y			= surface.getSize().height - notificationsRect.size.height - 8.0f;

		UiToolboxWindow window( surface, "notifications", notificationsRect, 5u );

		UiWidgetLayoutVertical mainLayout( window, 10.0f );
		mainLayout.setStretchOne();

		for( uintsize i = (uintsize)Notifications::Count - 1u; i < (uintsize)Notifications::Count; --i )
		{
			const Notifications notification = (Notifications)i;
			if( !m_notifications.isSet( notification ) )
			{
				continue;
			}

			UiWidget notificationWidget( window );
			notificationWidget.setStretchOne();
			notificationWidget.setPadding( UiBorder( 10.0f ) );

			UiColor bgColor = UiToolboxTheme::getColor( ImUiToolboxColor_Button );
			bgColor.alpha = 196u;

			notificationWidget.drawSkin( UiToolboxTheme::getSkin( ImUiToolboxSkin_Popup ), bgColor );

			UiToolboxConfigColorScope colorScope( ImUiToolboxColor_Text, UiColor::White );
			switch( notification )
			{
			case Notifications::Loaded:
				window.label( "Loaded!" );
				break;

			case Notifications::Saved:
				window.label( "Saved!" );
				break;

			case Notifications::Compiled:
				window.label( "Compiled!" );
				break;

			case Notifications::Count:
				break;
			}

			UiAnimation< float > animation( notificationWidget, 1.0f, 0.0f, 2.5f );

			UiWidget barWidget( window );
			barWidget.setMargin( UiBorder( 25.0f, 0.0f, 0.0f, 0.0f ) );
			barWidget.setFixedHeight( 2.0f );
			barWidget.setHStretch( animation.getValue() );

			barWidget.drawColor( UiToolboxTheme::getColor( ImUiToolboxColor_Text ) );

			if( animation.getValue() == 0.0f )
			{
				m_notifications.unset( notification );
			}
		}
	}

	void ResourceTool::doView( ImAppContext* imapp, UiToolboxWindow& window )
	{
		UiWidget viewWidget( window, (ImUiId)m_selecedEntry + 564646u );
		viewWidget.setStretchOne();
		viewWidget.setLayoutVertical( 4.0f );

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
			DynamicString& name = resource.getName();
			char* buffer = name.beginWrite( name.getLength() + 2u );
			if( window.textEdit( buffer, name.getCapacity() ) )
			{
				resource.increaseRevision();
				updateResourceNamesByType();
			}
			name.endWrite();
		}

		switch( resource.getType() )
		{
		case ResourceType::Image:
			doViewImage( imapp, window, resource );
			break;

		case ResourceType::Font:
			doViewFont( imapp, window, resource );
			break;

		case ResourceType::Skin:
			doViewSkin( imapp, window, resource );
			break;

		case ResourceType::Theme:
			doViewTheme( imapp, window, resource );
			break;
		}
	}

	void ResourceTool::doViewPackage( UiToolboxWindow& window )
	{
		window.label( "Name:" );
		{
			DynamicString& name = m_package.getName();
			char* buffer = name.beginWrite( name.getLength() + 2u );
			if( window.textEdit( buffer, name.getCapacity() ) )
			{
				m_package.increaseRevision();
			}
			name.endWrite();
		}

		window.label( "Output Name(extension added automatically):" );
		{
			DynamicString& outputPath = m_package.getOutputPath();
			char* buffer = outputPath.beginWrite( 32u );
			if( window.textEdit( buffer, outputPath.getCapacity() ) )
			{
				m_package.increaseRevision();
			}
			outputPath.endWrite();
		}

		window.checkBox( m_package.getExportCode(), "Output Code" );
	}

	void ResourceTool::doViewImage( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource )
	{
		window.label( "Path:" );
		{
			DynamicString& filePath = resource.getFileSourcePath();
			char* buffer = filePath.beginWrite( filePath.getLength() + 2u );
			if( window.textEdit( buffer, filePath.getCapacity() ) )
			{
				resource.increaseRevision();
			}
			filePath.endWrite();
		}

		{
			UiWidgetLayoutHorizontal flagsLayout( window, 4.0f );

			bool allowAtlas = resource.getImageAllowAtlas();
			window.checkBox( allowAtlas, "Allow Atlas" );
			resource.setImageAllowAtlas( allowAtlas );

			bool repeat = resource.getImageRepeat();
			window.checkBox( repeat, "Repeat" );
			resource.setImageRepeat( repeat );
		}

		const ImUiImage image = ImAppImageGetImage( resource.getOrCreateImage( imapp ) );

		{
			ImageViewWidget imageView( imapp, window );

			UiWidget imageWidget( window );
			imageWidget.setFixedSize( (UiSize)image * imageView.getZoom() );

			imageWidget.drawImage( image );
		}

		if( window.buttonLabel( "Create Skin" ) )
		{
			DynamicString name = resource.getName();
			name = "skin_" + name;

			Resource& skinResource = m_package.addResource( name, ResourceType::Skin );
			updateResourceNamesByType();

			skinResource.setSkinImageName( resource.getName() );

			m_selecedEntry = m_package.getResourceCount() - 1u;
			m_overrideSelectedEntry = true;
		}
	}

	void ResourceTool::doViewFont( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource )
	{
		window.label( "Path:" );
		{
			DynamicString& filePath = resource.getFileSourcePath();
			char* buffer = filePath.beginWrite( filePath.getLength() + 2u );
			if( window.textEdit( buffer, filePath.getCapacity() ) )
			{
				resource.increaseRevision();
			}
			filePath.endWrite();
		}

		{
			UiWidgetLayoutHorizontal flagsLayout( window, 4.0f );
			flagsLayout.setHStretch( 1.0f );

			window.label( "Font Size:" );

			float fontSize = resource.getFontSize();

			{
				UiToolboxSlider sizeSlider( window, fontSize, 0.1f, 72.0f );
				sizeSlider.setHStretch( 4.0f );

				if( sizeSlider.end() )
				{
					resource.setFontSize( fontSize );
				}
			}

			if( doFloatTextEdit( window, fontSize, 1 ) )
			{
				resource.setFontSize( fontSize );
			}
		}

		bool isScalable = resource.getFontIsScalable();
		window.checkBox( isScalable, "Is Scalable" );
		resource.setFontIsScalable( isScalable );

		window.label( "Unicode Blocks:" );

		Resource::FontUnicodeBlockArray& blocks = resource.getFontUnicodeBlocks();
		if( blocks.isEmpty() )
		{
			window.label( "No Codepoint Blocks." );
		}
		else
		{
			UiWidgetLayoutGrid grid( window, 4u, 4.0f, 4.0f );
			grid.setStretchOne();

			ResourceFontUnicodeBlock* blockToRemove = nullptr;
			for( ResourceFontUnicodeBlock& block : blocks )
			{
				bool chnaged = false;
				chnaged |= doUIntTextEdit( window, block.first );
				chnaged |= doUIntTextEdit( window, block.last );

				char* buffer = block.name.beginWrite( 32u );
				if( window.textEdit( buffer, block.name.getCapacity() ) )
				{
					chnaged = true;
				}
				block.name.endWrite();

				if( window.buttonLabel( "Remove" ) )
				{
					blockToRemove = &block;
					chnaged = true;
				}

				if( chnaged )
				{
					resource.increaseRevision();
				}
			}

			if( blockToRemove )
			{
				blocks.eraseSorted( blockToRemove );
			}
		}

		{
			UiWidgetLayoutHorizontal blocksLayout( window, 4.0f );
			blocksLayout.setHStretch( 1.0f );

			const ConstArrayView< ResourceUnicodeBlock > unicodeBlocks = getUnicodeBlocks();

			size_t selectedUnicodeBlock;
			{
				toolbox::UiToolboxDropdown blocksDropdown( window, &unicodeBlocks.getData()->name, unicodeBlocks.getLength(), unicodeBlocks.getElementSizeInBytes() );
				blocksDropdown.setHStretch( 1.0f );
				selectedUnicodeBlock = blocksDropdown.getSelectedIndex();
			}

			if( window.buttonLabel( "Add Unicode Block" ) )
			{
				const ResourceUnicodeBlock& sourceBlock = unicodeBlocks[ selectedUnicodeBlock ];

				ResourceFontUnicodeBlock& targetBlock = blocks.pushBack();
				targetBlock.first	= sourceBlock.first;
				targetBlock.last	= sourceBlock.last;
				targetBlock.name	= sourceBlock.name;
			}

			if( window.buttonLabel( "Add custom Block" ) )
			{
				blocks.pushBack();
			}
		}
	}

	void ResourceTool::doViewSkin( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource )
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
			const ImUiImage image = ImAppImageGetImage( imageResource->getOrCreateImage( imapp ) );

			UiBorder& skinBorder = resource.getSkinBorder();
			{
				UiWidgetLayoutHorizontal skinLayout( window, 4.0f );
				skinLayout.setHStretch( 1.0f );

				struct BorderInfo { const char* title; float& value; } borders[] =
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
					borderLayout.setHStretch( 1.0f );

					window.label( border.title );

					float newValue = border.value;
					if( window.slider( newValue, 0.0f, (float)image.height ) )
					{
						newValue = floorf( newValue );
						if( newValue != border.value )
						{
							border.value = newValue;
							resource.increaseRevision();
						}
					}

					if( doFloatTextEdit( window, border.value, 0 ) )
					{
						resource.increaseRevision();
					}
				}
			}

			const bool previewSkin = window.checkBoxState( "Preview Skin" );

			ImageViewWidget imageView( imapp, window );

			UiWidget imageWidget( window );
			imageWidget.setFixedSize( (UiSize)image * imageView.getZoom() );

			if( previewSkin )
			{
				ImUiSkin skin;
				skin.textureHandle	= image.textureHandle;
				skin.width			= image.width;
				skin.height			= image.height;
				skin.border			= skinBorder;
				skin.uv				= image.uv;

				imageWidget.drawSkin( skin, UiColor::White );
			}
			else
			{
				imageWidget.drawImage( image );

				UiBorder drawBorder = skinBorder;
				drawBorder.top *= imageView.getZoom();
				drawBorder.left *= imageView.getZoom();
				drawBorder.right *= imageView.getZoom();
				drawBorder.bottom *= imageView.getZoom();

				const UiSize imageSize = imageWidget.getSize();
				const UiRect innerRect = UiRect( UiPos::Zero, imageSize ).shrinkBorder( drawBorder );
				const UiColor color = UiColor( (uint8)0x30u, 0x90u, 0xe0u );
				imageWidget.drawLine( UiPos( 0.0f, innerRect.pos.y ), UiPos( imageSize.width, innerRect.pos.y ), color );
				imageWidget.drawLine( UiPos( innerRect.pos.x, 0.0f ), UiPos( innerRect.pos.x, imageSize.height ), color );
				imageWidget.drawLine( UiPos( innerRect.getRight(), 0.0f ), UiPos( innerRect.getRight(), imageSize.height ), color );
				imageWidget.drawLine( UiPos( 0.0f, innerRect.getBottom() ), UiPos( imageSize.width, innerRect.getBottom() ), color );
			}
		}
		else
		{
			window.labelFormat( "No Image selected!" );
		}
	}

	void ResourceTool::doViewTheme( ImAppContext* imapp, UiToolboxWindow& window, Resource& resource )
	{
		UiToolboxScrollArea scrollArea( window );
		scrollArea.setStretchOne();
		scrollArea.enableSpacing( true, false );

		UiWidgetLayoutVertical scrollLayout( window );
		scrollLayout.setHStretch( 1.0f );
		scrollLayout.setLayoutVertical( 4.0f );

		ThemeState* state = scrollLayout.newState< ThemeState >();

		UiWidget groupWidget;

		uintsize skipLevel = 0u;
		ResourceTheme& theme = resource.getTheme();
		for( ResourceThemeField& field : theme.getFields() )
		{
			if( skipLevel > 0u && field.level >= skipLevel )
			{
				continue;
			}
			else
			{
				skipLevel = 0u;
			}

			if( !field.uiField )
			{
				groupWidget.endWidget();

				groupWidget.beginWidget( window );
				groupWidget.setHStretch( 1.0f );
				groupWidget.setLayoutGrid( 2u, 0.0f, 4.0f );
				groupWidget.setPadding( UiBorder( 4.0f ) );
				groupWidget.setMargin( UiBorder( 0.0f, 20.0f * field.level, 0.0f, 0.0f ) );

				groupWidget.drawSkin( UiToolboxTheme::getSkin( ImUiToolboxSkin_ListItem ), UiToolboxTheme::getColor( ImUiToolboxColor_Button ) );

				bool isHover = false;
				bool wasClicked = false;
				ImUiWidgetInputState inputState;
				{
					UiWidget title( window );
					title.setLayoutHorizontal( 4.0f );
					title.setFixedWidth( state->maxWidth );

					{
						UiWidget icon( window );
						const ImUiImage iconImage = ImUiToolboxThemeGet()->icons[ field.open ? ImUiToolboxIcon_DropDownClose : ImUiToolboxIcon_DropDownOpen ];
						icon.setFixedSize( ImUiSizeCreateImage( &iconImage ) );
						icon.setHAlign( 1.0f );
						icon.setVAlign( 0.5f );

						icon.drawImage( iconImage, ImUiToolboxThemeGet()->colors[ ImUiToolboxColor_Text ] );
					}

					window.label( field.name );

					title.getInputState( inputState );
					isHover = inputState.isMouseOver;
					wasClicked = inputState.wasPressed && inputState.hasMouseReleased;
				}

				UiWidget strecher( window );
				strecher.setStretchOne();

				strecher.getInputState( inputState );
				isHover |= inputState.isMouseOver;
				wasClicked |= inputState.wasPressed && inputState.hasMouseReleased;

				if( isHover )
				{
					ImUiInputSetMouseCursor( strecher.getContext().getInternal(), ImUiInputMouseCursor_Hand );
				}

				if( wasClicked )
				{
					field.open = !field.open;
				}

				if( !field.open )
				{
					skipLevel = field.level + 1u;
				}

				continue;
			}
			else
			{
				UiToolboxLabel label( window, field.name );

				state->maxWidth = max( state->maxWidth, label.getMinSize().width + 4.0f );
				label.setFixedWidth( state->maxWidth );
			}

			switch( field.uiField->type )
			{
			case ImUiToolboxThemeReflectionType_Font:
				{
					DynamicString& value = theme.getFieldString( field );
					const StringView fontName = doResourceSelect( window, ResourceType::Font, value );
					if( value != fontName )
					{
						value = fontName;
						resource.increaseRevision();
					}
				}
				break;

			case ImUiToolboxThemeReflectionType_Color:
				{
					UiWidgetLayoutHorizontal colorLayout( window, 4.0f );
					colorLayout.setHStretch( 1.0f );

					ImUiColor& value = theme.getFieldColor( field );
					{
						UiWidget previewWidget( window );
						previewWidget.setVStretch( 1.0f );
						previewWidget.setFixedWidth( 50.0f );

						previewWidget.drawColor( UiColor::Black );
						previewWidget.drawPartialColor( UiRect( UiPos::Zero, previewWidget.getSize() ).shrinkBorder( UiBorder( 2.0f ) ),  (UiColor)value );
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

			case ImUiToolboxThemeReflectionType_Skin:
				{
					UiWidgetLayoutHorizontal skinLayout( window, 4.0f );
					skinLayout.setHStretch( 1.0f );

					DynamicString& value = theme.getFieldString( field );
					{
						UiWidget previewWidget( window );
						previewWidget.setVStretch( 1.0f );
						previewWidget.setFixedWidth( 50.0f );

						Resource* skinResource = m_package.findResource( value );
						Resource* imageResource = nullptr;
						if( skinResource )
						{
							imageResource = m_package.findResource( skinResource->getSkinImageName() );
						}

						if( skinResource && imageResource )
						{
							const ImUiImage image = ImAppImageGetImage( imageResource->getOrCreateImage( imapp ) );

							ImUiSkin skin;
							skin.textureHandle	= image.textureHandle;
							skin.width			= image.width;
							skin.height			= image.height;
							skin.border			= skinResource->getSkinBorder();
							skin.uv				= image.uv;
							previewWidget.drawColor( UiColor::Black );
							previewWidget.drawPartialSkin( UiRect( UiPos::Zero, previewWidget.getSize() ).shrinkBorder( UiBorder( 2.0f ) ), skin, UiColor::White );
						}
						else
						{
							previewWidget.drawColor( UiColor( (uint8)0xffu, 0u, 0u ) );
						}
					}

					const StringView skinName = doResourceSelect( window, ResourceType::Skin, value );
					if( value != skinName )
					{
						value = skinName;
						resource.increaseRevision();
					}
				}
				break;

			case ImUiToolboxThemeReflectionType_Float:
				{
					float& value = theme.getFieldFloat( field );
					if( doFloatTextEdit( window, value, 1 ) )
					{
						resource.increaseRevision();
					}
				}
				break;

			case ImUiToolboxThemeReflectionType_Double:
				{
					double& value = theme.getFieldDouble( field );
					float floatValue = (float)value;
					if( doFloatTextEdit( window, floatValue, 3 ) )
					{
						value = (double)floatValue;
					}
				}
				break;

			case ImUiToolboxThemeReflectionType_Border:
				{
					UiWidgetLayoutGrid skinLayout( window, 4u, 4.0f, 4.0f );
					skinLayout.setHStretch( 1.0f );

					window.label( "Top" );
					window.label( "Left" );
					window.label( "Right" );
					window.label( "Bottom" );

					ImUiBorder& value = theme.getFieldBorder( field );
					if( doFloatTextEdit( window, value.top, 0 ) ||
						doFloatTextEdit( window, value.left, 0 ) ||
						doFloatTextEdit( window, value.right, 0 ) ||
						doFloatTextEdit( window, value.bottom, 0 ) )
					{
						resource.increaseRevision();
					}
				}
				break;

			case ImUiToolboxThemeReflectionType_Size:
				{
					UiWidgetLayoutGrid skinLayout( window, 2u, 4.0f, 4.0f );
					skinLayout.setHStretch( 1.0f );

					window.label( "Width" );
					window.label( "Height" );

					ImUiSize& value = theme.getFieldSize( field );
					if( doFloatTextEdit( window, value.width, 0 ) ||
						doFloatTextEdit( window, value.height, 0 ) )
					{
						resource.increaseRevision();
					}
				}
				break;

			case ImUiToolboxThemeReflectionType_Image:
				{
					UiWidgetLayoutHorizontal imageLayout( window, 4.0f );
					imageLayout.setHStretch( 1.0f );

					DynamicString& value = theme.getFieldString( field );
					{
						UiWidget previewWidget( window );
						previewWidget.setVStretch( 1.0f );
						previewWidget.setFixedWidth( 50.0f );

						Resource* imageResource = m_package.findResource( value );
						if( imageResource )
						{
							const ImUiImage image = ImAppImageGetImage( imageResource->getOrCreateImage( imapp ) );

							const float width = (previewWidget.getRect().size.height / float( image.height )) * float( image.width );
							previewWidget.setFixedWidth( width );

							previewWidget.drawImage( image );
						}
						else
						{
							previewWidget.drawColor( UiColor( (uint8)0xffu, 0u, 0u ) );
						}
					}

					const StringView imageName = doResourceSelect( window, ResourceType::Image, value );
					if( value != imageName )
					{
						value = imageName;
						resource.increaseRevision();
					}
				}
				break;

			case ImUiToolboxThemeReflectionType_UInt32:
				{
					uint32& value = theme.getFieldUInt32( field );
					doUIntTextEdit( window, value );
				}
				break;
			}
		}
	}

	bool ResourceTool::doUIntTextEdit( UiToolboxWindow& window, uint32& value )
	{
		struct FloatEditState
		{
			uint32						lastValue;
			StaticArray< char, 32u >	buffer;
		};

		UiToolboxTextEdit textEdit( window );

		bool isNew;
		FloatEditState* state = textEdit.newState< FloatEditState >( isNew );
		if( isNew || state->lastValue != value )
		{
			snprintf( state->buffer.getData(), state->buffer.getLength(), "%u", value );
		}

		textEdit.setBuffer( state->buffer.getData(), state->buffer.getLength() );

		if( textEdit.end() )
		{
			string_tools::tryParseUInt32( value, StringView( state->buffer.getData() ) );
			state->lastValue = value;
			return true;
		}

		return false;
	}

	bool ResourceTool::doFloatTextEdit( UiToolboxWindow& window, float& value, uintsize decimalNumbers )
	{
		TIKI_ASSERT( decimalNumbers < 10u );

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
			char format[] = "%.0f";
			format[ 2u ] = '0' + (char)decimalNumbers;
			snprintf( state->buffer.getData(), state->buffer.getLength(), format, value );
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
		const ConstArrayView< const char* > resourceNames = m_resourceNamesByType[ (uintsize)type ];

		uintsize selectedIndex = 0u;
		if( selectedResourceName.isSet() )
		{
			for( uintsize i = 1u; i < resourceNames.getLength(); ++i )
			{
				const char* resourceName = resourceNames[ i ];
				if( selectedResourceName == resourceName )
				{
					selectedIndex = i;
					break;
				}
			}
		}

		UiToolboxDropdown resourceSelect( window, (const char**)resourceNames.getData(), resourceNames.getLength() );
		resourceSelect.setHStretch( 1.0f );

		const uintsize newSelectedIndex = resourceSelect.getSelectedIndex();
		if( newSelectedIndex > 0u )
		{
			if( newSelectedIndex < resourceNames.getLength() )
			{
				return (StringView)resourceNames[ newSelectedIndex ];
			}
			else
			{
				resourceSelect.setSelectedIndex( selectedIndex );
				return selectedResourceName;
			}
		}

		return "";
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

		ResourceType type = ResourceType::Count;
		const DynamicString ext = path.getExtension();
		if( ext == ".png" )
		{
			type = ResourceType::Image;
		}
		else if( ext == ".ttf" )
		{
			type = ResourceType::Font;
		}
		else
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

		Resource& resource = m_package.addResource( filename, type );
		updateResourceNamesByType();

		resource.getFileSourcePath().assign( remainingPath );

		m_selecedEntry = m_package.getResourceCount();
		m_overrideSelectedEntry = true;
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
		for( DynamicArray< const char* >& array : m_resourceNamesByType )
		{
			array.clear();
			array.pushBack( "- None -" );
		}

		for( const Resource* resource : m_package.getResources() )
		{
			m_resourceNamesByType[ (uintsize)resource->getType() ].pushBack( resource->getName() );
		}
	}

	ResourceTool::ImageViewWidget::ImageViewWidget( ImAppContext* imapp, UiToolboxWindow& window )
		: UiWidget( window )
		, m_scrollArea( window )
		, m_scrollContent( window )
	{
		setStretchOne();

		bool isNew;
		m_state = newState< ScrollState >( isNew );

		m_scrollArea.setStretchOne();
		m_scrollArea.setLayoutScroll( m_state->offset.x, m_state->offset.y );

		m_bgImage = ImAppResPakGetImage( ImAppResourceGetDefaultPak( imapp ), "transparent_bg" );
		if( m_bgImage && m_state->bgColor.alpha == 0.0f )
		{
			const UiSize size = getSize();

			ImUiImage image = *m_bgImage;
			image.uv.u0		= m_state->offset.x / image.width;
			image.uv.v0		= m_state->offset.y / image.height;
			image.uv.u1		= image.uv.u0 + (size.width / image.width);
			image.uv.v1		= image.uv.v0 + (size.height / image.height);

			m_scrollArea.drawImage( image );
		}
		else
		{
			m_scrollArea.drawColor( m_state->bgColor );
		}

		ImUiWidgetInputState widgetInput;
		m_scrollArea.getInputState( widgetInput );

		m_scrollContent.setAlign( 0.5f, 0.5f );

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

		UiWidgetLayoutHorizontal buttonsLayout( window, 5.0f );
		buttonsLayout.setPadding( UiBorder( 4.0f ) );

		buttonsLayout.drawColor( UiColor::createBlack( 0xa0u ) );

		{
			UiToolboxConfigColorScope colorScope( ImUiToolboxColor_Text, UiColor::White );
			window.labelFormat( "Zoom: %.0f%%", m_state->zoom * 100.0f );
		}

		doBackgroundButton( UiColor::TransparentBlack );
		doBackgroundButton( UiColor::Black );
		doBackgroundButton( UiColor::createGray( 128u ) );
		doBackgroundButton( UiColor::White );
	}

	void ResourceTool::ImageViewWidget::doBackgroundButton( const UiColor& color )
	{
		UiSize size = UiSize( 16.0f );
		if( m_bgImage )
		{
			size = UiSize( *m_bgImage );
		}

		UiToolboxButton bgButton( getWindow() );
		bgButton.setFixedSize( size );

		if( m_bgImage && color.alpha == 0.0f )
		{
			bgButton.drawImage( *m_bgImage );
		}
		else
		{
			bgButton.drawColor( color );
		}

		if( bgButton.end() )
		{
			m_state->bgColor = color;
		}
	}

}
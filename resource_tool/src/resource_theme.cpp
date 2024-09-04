#include "resource_theme.h"

#include <tiki/tiki_string_tools.h>

#include <tinyxml2.h>

namespace imapp
{
	using namespace tiki;

	ResourceTheme::ResourceTheme()
		: m_config( nullptr )
	{
		TIKI_STATIC_ASSERT( sizeof( m_config ) == 1256u );

		setFields();
	}

	ResourceTheme::ResourceTheme( const ResourceTheme& theme )
	{
		*this = theme;
	}

	bool ResourceTheme::load( XMLElement* resourceNode )
	{
		for( XMLElement* configNode = resourceNode->FirstChildElement( "value" ); configNode != nullptr; configNode = configNode->NextSiblingElement( "value" ) )
		{
			const char* name;
			if( configNode->QueryStringAttribute( "name", &name ) != XML_SUCCESS )
			{
				continue;
			}

			for( ResourceThemeField& field : m_fields )
			{
				if( field.type == ResourceThemeFieldType::Group ||
					field.name != name )
				{
					continue;
				}

				if( field.xml )
				{
					resourceNode->DeleteChild( field.xml );
				}

				field.xml = configNode;

				bool ok = true;
				switch( field.type )
				{
				case ResourceThemeFieldType::Group:
					break;

				case ResourceThemeFieldType::Font:
					{
						const char* fontName;
						if( configNode->QueryStringAttribute( "fontName", &fontName ) == XML_SUCCESS )
						{
							*field.data.fontNamePtr = fontName;
						}
						else
						{
							ok = false;
						}
					}
					break;

				case ResourceThemeFieldType::Color:
					{
						const char* colorHex;
						if( configNode->QueryStringAttribute( "color", &colorHex ) == XML_SUCCESS )
						{
							parseUiColor( *field.data.colorPtr, StringView( colorHex ) );
						}
						else
						{
							ok = false;
						}
					}
					break;

				case ResourceThemeFieldType::Skin:
					{
						const char* skinName;
						if( configNode->QueryStringAttribute( "skinName", &skinName ) == XML_SUCCESS )
						{
							*field.data.skinNamePtr = skinName;
						}
						else
						{
							ok = false;
						}
					}
					break;

				case ResourceThemeFieldType::Float:
					ok &= configNode->QueryFloatAttribute( "value", field.data.floatPtr ) == XML_SUCCESS;
					break;

				case ResourceThemeFieldType::Border:
					ok &= configNode->QueryFloatAttribute( "top", &field.data.borderPtr->top ) == XML_SUCCESS;
					ok &= configNode->QueryFloatAttribute( "left", &field.data.borderPtr->left ) == XML_SUCCESS;
					ok &= configNode->QueryFloatAttribute( "right", &field.data.borderPtr->right ) == XML_SUCCESS;
					ok &= configNode->QueryFloatAttribute( "bottom", &field.data.borderPtr->bottom ) == XML_SUCCESS;
					break;

				case ResourceThemeFieldType::Size:
					ok &= configNode->QueryFloatAttribute( "width", &field.data.sizePtr->width ) == XML_SUCCESS;
					ok &= configNode->QueryFloatAttribute( "height", &field.data.sizePtr->height ) == XML_SUCCESS;
					break;

				case ResourceThemeFieldType::Image:
					{
						const char* imageName;
						if( configNode->QueryStringAttribute( "imageName", &imageName ) == XML_SUCCESS )
						{
							*field.data.imageNamePtr = imageName;
						}
					}
					break;

				case ResourceThemeFieldType::UInt32:
					ok &= configNode->QueryIntAttribute( "value", (int*)field.data.uintPtr ) == XML_SUCCESS;
					break;
				}
			}
		}

		return true;
	}

	void ResourceTheme::serialize( XMLElement* resourceNode )
	{
		for( ResourceThemeField& field : m_fields )
		{
			if( field.type == ResourceThemeFieldType::Group )
			{
				continue;
			}

			if( !field.xml )
			{
				field.xml = resourceNode->GetDocument()->NewElement( "value" );
				resourceNode->InsertEndChild( field.xml );
			}

			field.xml->SetAttribute( "name", field.name );

			switch( field.type )
			{
			case ResourceThemeFieldType::Group:
				break;

			case ResourceThemeFieldType::Font:
				field.xml->SetAttribute( "fontName", field.data.fontNamePtr->toConstCharPointer() );
				break;

			case ResourceThemeFieldType::Color:
				{
					char buffer[ 32u ];
					formatUiColor( buffer, sizeof( buffer ), *field.data.colorPtr);

					field.xml->SetAttribute( "color", buffer );
				}
				break;

			case ResourceThemeFieldType::Skin:
				field.xml->SetAttribute( "skinName", field.data.skinNamePtr->toConstCharPointer() );
				break;

			case ResourceThemeFieldType::Float:
				field.xml->SetAttribute( "value", *field.data.floatPtr );
				break;

			case ResourceThemeFieldType::Border:
				field.xml->SetAttribute( "top", field.data.borderPtr->top );
				field.xml->SetAttribute( "left", field.data.borderPtr->left );
				field.xml->SetAttribute( "right", field.data.borderPtr->right );
				field.xml->SetAttribute( "bottom", field.data.borderPtr->bottom );
				break;

			case ResourceThemeFieldType::Size:
				field.xml->SetAttribute( "width", field.data.sizePtr->width );
				field.xml->SetAttribute( "height", field.data.sizePtr->height );
				break;

			case ResourceThemeFieldType::Image:
				field.xml->SetAttribute( "imageName", field.data.imageNamePtr->toConstCharPointer() );
				break;

			case ResourceThemeFieldType::UInt32:
				field.xml->SetAttribute( "value", *field.data.uintPtr );
				break;
			}
		}
	}

	ResourceTheme& ResourceTheme::operator=( const ResourceTheme& rhs )
	{
		m_config = rhs.m_config;
		m_fontName = rhs.m_fontName;

		m_skins = rhs.m_skins;
		m_icons = rhs.m_icons;

		setFields();

		return *this;
	}

	void ResourceTheme::setFields()
	{
		m_fields =
		{
			{ "Font",							ResourceThemeFieldType::Font,		{ &m_fontName } },
			{ "Text",							ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_Text ] } },

			// button
			{ "Button",							ResourceThemeFieldType::Group },

			{ "Button Skin",					ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_Button ] } },
			{ "Button Default",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_Button ] } },
			{ "Button Hover",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ButtonHover ] } },
			{ "Button Clocked",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ButtonClicked ] } },
			{ "Button Text",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ButtonText ] } },
			{ "Button Height",					ResourceThemeFieldType::Float,		{ &m_config.button.height } },
			{ "Button Padding",					ResourceThemeFieldType::Border,		{ &m_config.button.padding } },

			// check box
			{ "Check Box",						ResourceThemeFieldType::Group },

			{ "Check Box Unchecked Skin",		ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_CheckBox ] } },
			{ "Check Box Checked Skin",			ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_CheckBoxChecked ] } },
			{ "Check Box Default",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_CheckBox ] } },
			{ "Check Box Hover",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_CheckBoxHover ] } },
			{ "Check Box Clicked",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_CheckBoxClicked ] } },
			{ "Check Box Checked",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_CheckBoxChecked ] } },
			{ "Check Box Check Icon",			ResourceThemeFieldType::Image,		{ &m_icons[ ImUiToolboxIcon_CheckBoxChecked ] } },
			{ "Check Box Size",					ResourceThemeFieldType::Size,		{ &m_config.checkBox.size } },
			{ "Check Box Text Spacing",			ResourceThemeFieldType::Float,		{ &m_config.checkBox.textSpacing } },

			// slider
			{ "Slider",							ResourceThemeFieldType::Group },

			{ "Slider Skin",					ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_SliderBackground ] } },
			{ "Slider Pivot Skin",				ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_SliderPivot ] } },
			{ "Slider Background",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_SliderBackground ] } },
			{ "Slider Pivot",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_SliderPivot ] } },
			{ "Slider Pivot Hover",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_SliderPivotHover ] } },
			{ "Slider Pivot Clicked",			ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_SliderPivotClicked ] } },
			{ "Slider Height",					ResourceThemeFieldType::Float,		{ &m_config.slider.height } },
			{ "Slider Padding",					ResourceThemeFieldType::Border,		{ &m_config.slider.padding } },
			{ "Slider Pivot Size",				ResourceThemeFieldType::Size,		{ &m_config.slider.pivotSize } },

			// text edit
			{ "Text Edit",						ResourceThemeFieldType::Group },

			{ "Text Edit Skin",					ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_TextEditBackground ] } },
			{ "Text Edit Background",			ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_TextEditBackground ] } },
			{ "Text Edit Text",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_TextEditText ] } },
			{ "Text Edit Cursor",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_TextEditCursor ] } },
			{ "Text Edit Selection",			ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_TextEditSelection ] } },
			{ "Text Edit Height",				ResourceThemeFieldType::Float,		{ &m_config.textEdit.height } },
			{ "Text Edit Padding",				ResourceThemeFieldType::Border,		{ &m_config.textEdit.padding } },
			{ "Text Edit Cursor Size",			ResourceThemeFieldType::Size,		{ &m_config.textEdit.cursorSize } },
			{ "Text Edit Blink time",			ResourceThemeFieldType::Time,		{ &m_config.textEdit.blinkTime } },

			// progress bar
			{ "Progress Bar",					ResourceThemeFieldType::Group },

			{ "Progress Bar Skin",				ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ProgressBarBackground ] } },
			{ "Progress Bar Progress Skin",		ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ProgressBarProgress ] } },
			{ "Progress Bar Background",		ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ProgressBarBackground ] } },
			{ "Progress Bar Progress",			ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ProgressBarProgress ] } },
			{ "Progress Bar Height",			ResourceThemeFieldType::Float,		{ &m_config.progressBar.height } },
			{ "Progress Bar Padding",			ResourceThemeFieldType::Border,		{ &m_config.progressBar.padding } },

			// scroll area
			{ "Scroll Area",					ResourceThemeFieldType::Group },

			{ "Scroll Area Bar Skin",			ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ScrollAreaBarBackground ] } },
			{ "Scroll Area Bar Pivot Skin",		ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ScrollAreaBarPivot ] } },
			{ "Scroll Area Bar Background",		ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ScrollAreaBarBackground ] } },
			{ "Scroll Area Bar Pivot",			ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ScrollAreaBarPivot ] } },
			{ "Scroll Area Bar Size",			ResourceThemeFieldType::Float,		{ &m_config.scrollArea.barSize } },
			{ "Scroll Area Bar Spacing",		ResourceThemeFieldType::Float,		{ &m_config.scrollArea.barSpacing } },
			{ "Scroll Area Bar Min Size",		ResourceThemeFieldType::Size,		{ &m_config.scrollArea.barMinSize } },

			// list
			{ "List",							ResourceThemeFieldType::Group },

			{ "List Item",						ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ListItem ] } },
			{ "List Item Selected Skin",		ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ListItemSelected ] } },
			{ "List Item Hover",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ListItemHover ] } },
			{ "List Item Clicked",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ListItemClicked ] } },
			{ "List Item Selected",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ListItemSelected ] } },
			{ "List Item Spacing",				ResourceThemeFieldType::Float,		{ &m_config.list.itemSpacing } },

			// drop down
			{ "Drop Down",						ResourceThemeFieldType::Group },

			{ "Drop Down Skin",					ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_DropDown ] } },
			{ "Drop Down List Skin",			ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_DropDownList ] } },
			{ "Drop Down List Item Skin",		ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_DropDownListItem ] } },
			{ "Drop Down Background",			ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDown ] } },
			{ "Drop Down Text",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownText ] } },
			{ "Drop Down Icon",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownIcon ] } },
			{ "Drop Down Hover",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownHover ] } },
			{ "Drop Down Clicked",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownClicked ] } },
			{ "Drop Down Open",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownOpen ] } },
			{ "Drop Down List",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownList ] } },
			{ "Drop Down List Item Text",		ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownListItemText ] } },
			{ "Drop Down List Item Hover",		ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownListItemHover ] } },
			{ "Drop Down List Item Clicked",	ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownListItemClicked ] } },
			{ "Drop Down List Item Selected",	ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownListItemSelected ] } },
			{ "Drop Down Open Icon",			ResourceThemeFieldType::Image,		{ &m_icons[ ImUiToolboxIcon_DropDownOpenIcon ] } },
			{ "Drop Down Close Icon",			ResourceThemeFieldType::Image,		{ &m_icons[ ImUiToolboxIcon_DropDownCloseIcon ] } },
			{ "Drop Down Height",				ResourceThemeFieldType::Float,		{ &m_config.dropDown.height } },
			{ "Drop Down Padding",				ResourceThemeFieldType::Border,		{ &m_config.dropDown.padding } },
			{ "Drop Down List Z Order",			ResourceThemeFieldType::UInt32,		{ &m_config.dropDown.listZOrder } },
			{ "Drop Down List Margin",			ResourceThemeFieldType::Border,		{ &m_config.dropDown.listMargin } },
			{ "Drop Down List Max Length",		ResourceThemeFieldType::UInt32,		{ &m_config.dropDown.listMaxLength } },
			{ "Drop Down Item Padding",			ResourceThemeFieldType::Border,		{ &m_config.dropDown.itemPadding } },
			{ "Drop Down Item Size",			ResourceThemeFieldType::Float,		{ &m_config.dropDown.itemSize } },
			{ "Drop Down Item Spacing",			ResourceThemeFieldType::Float,		{ &m_config.dropDown.itemSpacing } },

			// pop-up
			{ "Pop-Up",							ResourceThemeFieldType::Group },

			{ "Popup Skin",						ResourceThemeFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_Popup ] } },
			{ "Popup Background",				ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_PopupBackground ] } },
			{ "Popup Window",					ResourceThemeFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_Popup ] } },
			{ "Pop-Up Z Order",					ResourceThemeFieldType::UInt32,		{ &m_config.popup.zOrder } },
			{ "Pop-Up Padding",					ResourceThemeFieldType::Border,		{ &m_config.popup.padding } },
			{ "Pop-Up Button Spacing",			ResourceThemeFieldType::Float,		{ &m_config.popup.buttonSpacing } },
		};
	}
}
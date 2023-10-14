#include "resource_toolbox_config.h"

#include "resource_package.h"

#include <tiki/tiki_string_tools.h>

namespace imapp
{
	using namespace tiki;

	ResourceToolboxConfig::ResourceToolboxConfig()
		: m_config( nullptr )
	{
		TIKI_STATIC_ASSERT( sizeof( m_config ) == 1128u );

		setFields();
	}

	ResourceToolboxConfig::ResourceToolboxConfig( const ResourceToolboxConfig& config )
	{
		*this = config;
	}

	bool ResourceToolboxConfig::load( XMLElement* resourceNode )
	{
		for( XMLElement* configNode = resourceNode->FirstChildElement( "config" ); configNode != nullptr; configNode = configNode->NextSiblingElement( "config" ) )
		{
			const char* name;
			if( !configNode->QueryStringAttribute( "name", &name ) != XML_SUCCESS )
			{
				continue;
			}

			for( ResourceToolboxConfigField& field : m_fields )
			{
				if( field.name != name )
				{
					continue;
				}

				field.xml = configNode;

				bool ok = true;
				switch( field.type )
				{
				case ResourceToolboxConfigFieldType::Group:
					break;

				case ResourceToolboxConfigFieldType::Font:
					{
						const char* fontName;
						if( configNode->QueryStringAttribute( "fontName", &fontName ) == XML_SUCCESS )
						{
							*field.data.fontNamePtr = fontName;
						}
					}
					break;

				case ResourceToolboxConfigFieldType::Color:
					{
						const char* colorHex;
						if( configNode->QueryStringAttribute( "color", &colorHex ) == XML_SUCCESS )
						{
							parseUiColor( *field.data.colorPtr, StringView( colorHex ) );
						}
					}
					break;

				case ResourceToolboxConfigFieldType::Skin:
					{
						const char* skinName;
						if( configNode->QueryStringAttribute( "skinName", &skinName ) == XML_SUCCESS )
						{
							*field.data.skinNamePtr = skinName;
						}
					}
					break;

				case ResourceToolboxConfigFieldType::Float:
					ok &= configNode->QueryFloatAttribute( "value", field.data.floatPtr ) == XML_SUCCESS;
					break;

				case ResourceToolboxConfigFieldType::Border:
					ok &= configNode->QueryFloatAttribute( "top", &field.data.borderPtr->top ) == XML_SUCCESS;
					ok &= configNode->QueryFloatAttribute( "left", &field.data.borderPtr->left ) == XML_SUCCESS;
					ok &= configNode->QueryFloatAttribute( "right", &field.data.borderPtr->right ) == XML_SUCCESS;
					ok &= configNode->QueryFloatAttribute( "bottom", &field.data.borderPtr->bottom ) == XML_SUCCESS;
					break;

				case ResourceToolboxConfigFieldType::Size:
					ok &= configNode->QueryFloatAttribute( "width", &field.data.sizePtr->width ) == XML_SUCCESS;
					ok &= configNode->QueryFloatAttribute( "height", &field.data.sizePtr->height ) == XML_SUCCESS;
					break;

				case ResourceToolboxConfigFieldType::Image:
					{
						const char* imageName;
						if( configNode->QueryStringAttribute( "imageName", &imageName ) == XML_SUCCESS )
						{
							*field.data.imageNamePtr = imageName;
						}
					}
					break;

				case ResourceToolboxConfigFieldType::UInt32:
					ok = configNode->QueryIntAttribute( "value", (int*)field.data.uintPtr ) == XML_SUCCESS;
					break;
				}
			}
		}

		return true;
	}

	void ResourceToolboxConfig::serialize( XMLElement* resourceNode )
	{
		for( ResourceToolboxConfigField& field : m_fields )
		{
			if( field.type == ResourceToolboxConfigFieldType::Group )
			{
				continue;
			}

			if( !field.xml )
			{
				field.xml = resourceNode->GetDocument()->NewElement( "config" );
				resourceNode->InsertEndChild( field.xml );
			}

			field.xml->SetAttribute( "name", field.name );

			switch( field.type )
			{
			case ResourceToolboxConfigFieldType::Group:
				break;

			case ResourceToolboxConfigFieldType::Font:
				field.xml->SetAttribute( "fontName", field.data.fontNamePtr->toConstCharPointer() );
				break;

			case ResourceToolboxConfigFieldType::Color:
				{
					char buffer[ 32u ];
					formatUiColor( buffer, sizeof( buffer ), *field.data.colorPtr);

					field.xml->SetAttribute( "color", buffer );
				}
				break;

			case ResourceToolboxConfigFieldType::Skin:
				field.xml->SetAttribute( "skinName", field.data.skinNamePtr->toConstCharPointer() );
				break;

			case ResourceToolboxConfigFieldType::Float:
				field.xml->SetAttribute( "value", *field.data.floatPtr );
				break;

			case ResourceToolboxConfigFieldType::Border:
				field.xml->SetAttribute( "top", field.data.borderPtr->top );
				field.xml->SetAttribute( "left", field.data.borderPtr->left );
				field.xml->SetAttribute( "right", field.data.borderPtr->right );
				field.xml->SetAttribute( "bottom", field.data.borderPtr->bottom );
				break;

			case ResourceToolboxConfigFieldType::Size:
				field.xml->SetAttribute( "width", field.data.sizePtr->width );
				field.xml->SetAttribute( "height", field.data.sizePtr->height );
				break;

			case ResourceToolboxConfigFieldType::Image:
				field.xml->SetAttribute( "imageName", field.data.imageNamePtr->toConstCharPointer() );
				break;

			case ResourceToolboxConfigFieldType::UInt32:
				field.xml->SetAttribute( "value", *field.data.uintPtr );
				break;
			}
		}
	}

	Resource* ResourceToolboxConfig::findFont( ResourcePackage& package )
	{
		return package.findResource( m_fontName );
	}

	ResourceToolboxConfig& ResourceToolboxConfig::operator=( const ResourceToolboxConfig& rhs )
	{
		m_config = rhs.m_config;
		m_fontName = rhs.m_fontName;

		m_skins = rhs.m_skins;
		m_images = rhs.m_images;

		setFields();

		return *this;
	}

	void ResourceToolboxConfig::setFields()
	{
		m_fields =
		{
			{ "Font",							ResourceToolboxConfigFieldType::Font,		{ &m_fontName } },
			{ "Text",							ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_Text ] } },

			// button
			{ "Button",							ResourceToolboxConfigFieldType::Group },

			{ "Button",							ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_Button ] } },
			{ "Button",							ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_Button ] } },
			{ "Button Hover",					ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ButtonHover ] } },
			{ "Button Clocked",					ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ButtonClicked ] } },
			{ "Button Text",					ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ButtonText ] } },
			{ "Button Height",					ResourceToolboxConfigFieldType::Float,		{ &m_config.button.height } },
			{ "Button Padding",					ResourceToolboxConfigFieldType::Border,		{ &m_config.button.padding } },

			// check box
			{ "Check Box",						ResourceToolboxConfigFieldType::Group },

			{ "Check Box",						ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_CheckBox ] } },
			{ "Check Box Checked",				ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_CheckBoxChecked ] } },
			{ "Check Box",						ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_CheckBox ] } },
			{ "Check Box Hover",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_CheckBoxHover ] } },
			{ "Check Box Clicked",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_CheckBoxClicked ] } },
			{ "Check Box Checked",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_CheckBoxChecked ] } },
			{ "Check Box Checked",				ResourceToolboxConfigFieldType::Image,		{ &m_images[ ImUiToolboxImage_CheckBoxChecked ] } },
			{ "Check Box Size",					ResourceToolboxConfigFieldType::Size,		{ &m_config.checkBox.size } },
			{ "Check Box Text Spacing",			ResourceToolboxConfigFieldType::Float,		{ &m_config.checkBox.textSpacing } },

			// slider
			{ "Slider",							ResourceToolboxConfigFieldType::Group },

			{ "Slider Background",				ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_SliderBackground ] } },
			{ "Slider Pivot",					ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_SliderPivot ] } },
			{ "Slider Background",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_SliderBackground ] } },
			{ "Slider Pivot",					ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_SliderPivot ] } },
			{ "Slider PivotHover",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_SliderPivotHover ] } },
			{ "Slider PivotClicked",			ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_SliderPivotClicked ] } },
			{ "Slider Height",					ResourceToolboxConfigFieldType::Float,		{ &m_config.slider.height } },
			{ "Slider Padding",					ResourceToolboxConfigFieldType::Border,		{ &m_config.slider.padding } },
			{ "Slider Pivot Size",				ResourceToolboxConfigFieldType::Size,		{ &m_config.slider.pivotSize } },

			// text edit
			{ "Text Edit",						ResourceToolboxConfigFieldType::Group },

			{ "Text Edit Background",			ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_TextEditBackground ] } },
			{ "Text Edit Background",			ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_TextEditBackground ] } },
			{ "Text Edit Text",					ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_TextEditText ] } },
			{ "Text Edit Cursor",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_TextEditCursor ] } },
			{ "Text Edit Selection",			ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_TextEditSelection ] } },
			{ "Text Edit Height",				ResourceToolboxConfigFieldType::Float,		{ &m_config.textEdit.height } },
			{ "Text Edit Padding",				ResourceToolboxConfigFieldType::Border,		{ &m_config.textEdit.padding } },
			{ "Text Edit Cursor Size",			ResourceToolboxConfigFieldType::Size,		{ &m_config.textEdit.cursorSize } },
			{ "Text Edit Blink time",			ResourceToolboxConfigFieldType::Float,		{ &m_config.textEdit.blinkTime } },

			// progress bar
			{ "Progress Bar",					ResourceToolboxConfigFieldType::Group },

			{ "Progress Bar Background",		ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ProgressBarBackground ] } },
			{ "Progress Bar Progress",			ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ProgressBarProgress ] } },
			{ "Progress Bar Background",		ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ProgressBarBackground ] } },
			{ "Progress Bar Progress",			ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ProgressBarProgress ] } },
			{ "Progress Bar Height",			ResourceToolboxConfigFieldType::Float,		{ &m_config.progressBar.height } },
			{ "Progress Bar Padding",			ResourceToolboxConfigFieldType::Border,		{ &m_config.progressBar.padding } },

			// scroll area
			{ "Scroll Area",					ResourceToolboxConfigFieldType::Group },

			{ "Scroll Area Bar Background",		ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ScrollAreaBarBackground ] } },
			{ "Scroll Area Bar Pivot",			ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ScrollAreaBarPivot ] } },
			{ "Scroll Area Bar Background",		ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ScrollAreaBarBackground ] } },
			{ "Scroll Area Bar Pivot",			ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ScrollAreaBarPivot ] } },
			{ "Scroll Area Bar Size",			ResourceToolboxConfigFieldType::Float,		{ &m_config.scrollArea.barSize } },
			{ "Scroll Area Bar Spacing",		ResourceToolboxConfigFieldType::Border,		{ &m_config.scrollArea.barSpacing } },
			{ "Scroll Area Bar Min Size",		ResourceToolboxConfigFieldType::Size,		{ &m_config.scrollArea.barMinSize } },

			// list
			{ "List",							ResourceToolboxConfigFieldType::Group },

			{ "List Item",						ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_ListItem ] } },
			{ "List Item Hover",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ListItemHover ] } },
			{ "List Item Clicked",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ListItemClicked ] } },
			{ "List Item Selected",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_ListItemSelected ] } },
			{ "List Item Spacing",				ResourceToolboxConfigFieldType::Float,		{ &m_config.list.itemSpacing } },

			// scroll area
			{ "Scroll Area",					ResourceToolboxConfigFieldType::Group },

			{ "Scroll Area Bar Size",			ResourceToolboxConfigFieldType::Float,		{ &m_config.scrollArea.barSize } },
			{ "Scroll Area Bar Spacing",		ResourceToolboxConfigFieldType::Border,		{ &m_config.scrollArea.barSpacing } },
			{ "Scroll Area Bar Min Size",		ResourceToolboxConfigFieldType::Size,		{ &m_config.scrollArea.barMinSize } },

			// drop down
			{ "Drop Down",						ResourceToolboxConfigFieldType::Group },

			{ "Drop Down",						ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_DropDown ] } },
			{ "Drop Down List",					ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_DropDownList ] } },
			{ "Drop Down List Item",			ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_DropDownListItem ] } },
			{ "Drop Down ",						ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDown ] } },
			{ "Drop Down Text",					ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownText ] } },
			{ "Drop Down Hover",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownHover ] } },
			{ "Drop Down Clicked",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownClicked ] } },
			{ "Drop Down Open",					ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownOpen ] } },
			{ "Drop Down List",					ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownList ] } },
			{ "Drop Down List Item Text",		ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownListItemText ] } },
			{ "Drop Down List Item Hover",		ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownListItemHover ] } },
			{ "Drop Down List Item Clicked",	ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownListItemClicked ] } },
			{ "Drop Down List Item Selected",	ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_DropDownListItemSelected ] } },
			{ "Drop Down Open Icon",			ResourceToolboxConfigFieldType::Image,		{ &m_images[ ImUiToolboxImage_DropDownOpenIcon ] } },
			{ "Drop Down Close Icon",			ResourceToolboxConfigFieldType::Image,		{ &m_images[ ImUiToolboxImage_DropDownCloseIcon ] } },
			{ "Drop Down Height",				ResourceToolboxConfigFieldType::Float,		{ &m_config.dropDown.height } },
			{ "Drop Down Padding",				ResourceToolboxConfigFieldType::Border,		{ &m_config.dropDown.padding } },
			{ "Drop Down List Z Order",			ResourceToolboxConfigFieldType::UInt32,		{ &m_config.dropDown.listZOrder } },
			{ "Drop Down List Max Length",		ResourceToolboxConfigFieldType::UInt32,		{ &m_config.dropDown.maxListLength } },
			{ "Drop Down Item Padding",			ResourceToolboxConfigFieldType::Border,		{ &m_config.dropDown.itemPadding } },
			{ "Drop Down Item Size",			ResourceToolboxConfigFieldType::Float,		{ &m_config.dropDown.itemSize } },
			{ "Drop Down Item Spacing",			ResourceToolboxConfigFieldType::Float,		{ &m_config.dropDown.itemSpacing } },

			// pop-up
			{ "Pop-Up",							ResourceToolboxConfigFieldType::Group },

			{ "Popup",							ResourceToolboxConfigFieldType::Skin,		{ &m_skins[ ImUiToolboxSkin_Popup ] } },
			{ "Popup Background",				ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_PopupBackground ] } },
			{ "Popup",							ResourceToolboxConfigFieldType::Color,		{ &m_config.colors[ ImUiToolboxColor_Popup ] } },
			{ "Pop-Up Z Order",					ResourceToolboxConfigFieldType::UInt32,		{ &m_config.popup.zOrder } },
			{ "Pop-Up Padding",					ResourceToolboxConfigFieldType::Border,		{ &m_config.popup.padding } },
			{ "Pop-Up Button Spacing",			ResourceToolboxConfigFieldType::Float,		{ &m_config.popup.buttonSpacing } },
		};
	}
}
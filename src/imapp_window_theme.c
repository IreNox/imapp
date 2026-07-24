#include "imapp_window_theme.h"

#include "imapp_platform.h"

#include <assert.h>

static ImappWindowTheme s_windowTheme;

typedef struct ImappWindowThemeState
{
	ImuiPos		clickPosition;
	//ImuiPos		lastPosition;
} ImappWindowThemeState;

static const ImuiToolboxThemeReflectionField s_windowThemeReflectionFields[] =
{
	{ "Title/Active/Skin",									ImuiToolboxThemeReflectionType_Skin,	offsetof( ImappWindowTheme, titleActive.skin ) },
	{ "Title/Active/Text Color",							ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleActive.textColor ) },
	{ "Title/Active/Background Color",						ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleActive.backgroundColor ) },

	{ "Title/Inactive/Skin",								ImuiToolboxThemeReflectionType_Skin,	offsetof( ImappWindowTheme, titleInactive.skin ) },
	{ "Title/Inactive/Text Color",							ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleInactive.textColor ) },
	{ "Title/Inactive/Background Color",					ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleInactive.backgroundColor ) },

	{ "Title/Buttons/Minimize/Skin",						ImuiToolboxThemeReflectionType_Skin,	offsetof( ImappWindowTheme, titleMinimizeButton.skin ) },
	{ "Title/Buttons/Minimize/Size",						ImuiToolboxThemeReflectionType_Size,	offsetof( ImappWindowTheme, titleMinimizeButton.size ) },
	{ "Title/Buttons/Minimize/Background Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMinimizeButton.backgroundColor ) },
	{ "Title/Buttons/Minimize/Background Inactive Color",	ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMinimizeButton.backgroundColorInactive ) },
	{ "Title/Buttons/Minimize/Background Hover Color",		ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMinimizeButton.backgroundColorHover ) },
	{ "Title/Buttons/Minimize/Background Clicked Color",	ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMinimizeButton.backgroundColorClicked ) },
	{ "Title/Buttons/Minimize/Icon/Image",					ImuiToolboxThemeReflectionType_Image,	offsetof( ImappWindowTheme, titleMinimizeButton.icon ) },
	{ "Title/Buttons/Minimize/Icon/Size",					ImuiToolboxThemeReflectionType_Size,	offsetof( ImappWindowTheme, titleMinimizeButton.iconSize ) },
	{ "Title/Buttons/Minimize/Icon/Color",					ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMinimizeButton.iconColor ) },
	{ "Title/Buttons/Minimize/Icon/Inactive Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMinimizeButton.iconColorInactive ) },
	{ "Title/Buttons/Minimize/Icon/Hover Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMinimizeButton.iconColorHover ) },
	{ "Title/Buttons/Minimize/Icon/Clicked Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMinimizeButton.iconColorClicked ) },

	{ "Title/Buttons/Restore/Skin",							ImuiToolboxThemeReflectionType_Skin,	offsetof( ImappWindowTheme, titleRestoreButton.skin ) },
	{ "Title/Buttons/Restore/Size",							ImuiToolboxThemeReflectionType_Size,	offsetof( ImappWindowTheme, titleRestoreButton.size ) },
	{ "Title/Buttons/Restore/Background Color",				ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleRestoreButton.backgroundColor ) },
	{ "Title/Buttons/Restore/Background Inactive Color",	ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleRestoreButton.backgroundColorInactive ) },
	{ "Title/Buttons/Restore/Background Hover Color",		ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleRestoreButton.backgroundColorHover ) },
	{ "Title/Buttons/Restore/Background Clicked Color",		ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleRestoreButton.backgroundColorClicked ) },
	{ "Title/Buttons/Restore/Icon/Image",					ImuiToolboxThemeReflectionType_Image,	offsetof( ImappWindowTheme, titleRestoreButton.icon ) },
	{ "Title/Buttons/Restore/Icon/Size",					ImuiToolboxThemeReflectionType_Size,	offsetof( ImappWindowTheme, titleRestoreButton.iconSize ) },
	{ "Title/Buttons/Restore/Icon/Color",					ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleRestoreButton.iconColor ) },
	{ "Title/Buttons/Restore/Icon/Inactive Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleRestoreButton.iconColorInactive ) },
	{ "Title/Buttons/Restore/Icon/Hover Color",				ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleRestoreButton.iconColorHover ) },
	{ "Title/Buttons/Restore/Icon/Clicked Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleRestoreButton.iconColorClicked ) },

	{ "Title/Buttons/Maximize/Skin",						ImuiToolboxThemeReflectionType_Skin,	offsetof( ImappWindowTheme, titleMaximizeButton.skin ) },
	{ "Title/Buttons/Maximize/Size",						ImuiToolboxThemeReflectionType_Size,	offsetof( ImappWindowTheme, titleMaximizeButton.size ) },
	{ "Title/Buttons/Maximize/Background Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMaximizeButton.backgroundColor ) },
	{ "Title/Buttons/Maximize/Background Inactive Color",	ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMaximizeButton.backgroundColorInactive ) },
	{ "Title/Buttons/Maximize/Background Hover Color",		ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMaximizeButton.backgroundColorHover ) },
	{ "Title/Buttons/Maximize/Background Clicked Color",	ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMaximizeButton.backgroundColorClicked ) },
	{ "Title/Buttons/Maximize/Icon/Image",					ImuiToolboxThemeReflectionType_Image,	offsetof( ImappWindowTheme, titleMaximizeButton.icon ) },
	{ "Title/Buttons/Maximize/Icon/Size",					ImuiToolboxThemeReflectionType_Size,	offsetof( ImappWindowTheme, titleMaximizeButton.iconSize ) },
	{ "Title/Buttons/Maximize/Icon/Color",					ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMaximizeButton.iconColor ) },
	{ "Title/Buttons/Maximize/Icon/Inactive Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMaximizeButton.iconColorInactive ) },
	{ "Title/Buttons/Maximize/Icon/Hover Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMaximizeButton.iconColorHover ) },
	{ "Title/Buttons/Maximize/Icon/Clicked Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleMaximizeButton.iconColorClicked ) },

	{ "Title/Buttons/Close/Skin",							ImuiToolboxThemeReflectionType_Skin,	offsetof( ImappWindowTheme, titleCloseButton.skin ) },
	{ "Title/Buttons/Close/Size",							ImuiToolboxThemeReflectionType_Size,	offsetof( ImappWindowTheme, titleCloseButton.size ) },
	{ "Title/Buttons/Close/Background Color",				ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleCloseButton.backgroundColor ) },
	{ "Title/Buttons/Close/Background Inactive Color",		ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleCloseButton.backgroundColorInactive ) },
	{ "Title/Buttons/Close/Background Hover Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleCloseButton.backgroundColorHover ) },
	{ "Title/Buttons/Close/Background Clicked Color",		ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleCloseButton.backgroundColorClicked ) },
	{ "Title/Buttons/Close/Icon/Image",						ImuiToolboxThemeReflectionType_Image,	offsetof( ImappWindowTheme, titleCloseButton.icon ) },
	{ "Title/Buttons/Close/Icon/Size",						ImuiToolboxThemeReflectionType_Size,	offsetof( ImappWindowTheme, titleCloseButton.iconSize ) },
	{ "Title/Buttons/Close/Icon/Color",						ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleCloseButton.iconColor ) },
	{ "Title/Buttons/Close/Icon/Inactive Color",			ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleCloseButton.iconColorInactive ) },
	{ "Title/Buttons/Close/Icon/Hover Color",				ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleCloseButton.iconColorHover ) },
	{ "Title/Buttons/Close/Icon/Clicked Color",				ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, titleCloseButton.iconColorClicked ) },

	{ "Title/Padding",										ImuiToolboxThemeReflectionType_Border,	offsetof( ImappWindowTheme, titlePadding ) },
	{ "Title/Height",										ImuiToolboxThemeReflectionType_Float,	offsetof( ImappWindowTheme, titleHeight ) },
	{ "Title/Spacing",										ImuiToolboxThemeReflectionType_Float,	offsetof( ImappWindowTheme, titleSpacing ) },

	{ "Body/Active/Skin",									ImuiToolboxThemeReflectionType_Skin,	offsetof( ImappWindowTheme, bodyActive.skin ) },
	{ "Body/Active/Color",									ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, bodyActive.color ) },

	{ "Body/Inactive/Skin",									ImuiToolboxThemeReflectionType_Skin,	offsetof( ImappWindowTheme, bodyInactive.skin ) },
	{ "Body/Inactive/Color",								ImuiToolboxThemeReflectionType_Color,	offsetof( ImappWindowTheme, bodyInactive.color ) },

	{ "Body/Padding",										ImuiToolboxThemeReflectionType_Border,	offsetof( ImappWindowTheme, bodyPadding ) },
};
static_assert( sizeof( ImappWindowTheme ) == 776u, "window theme changed" );

ImuiToolboxThemeReflection imappWindowThemeReflectionGet()
{
	ImuiToolboxThemeReflection reflection;
	reflection.fields	= s_windowThemeReflectionFields;
	reflection.count	= IMAPP_ARRAY_COUNT( s_windowThemeReflectionFields );

	return reflection;
}

ImappWindowTheme* imappWindowThemeGet()
{
	return &s_windowTheme;
}

void imappWindowThemeSet( const ImappWindowTheme* windowTheme )
{
	s_windowTheme = *windowTheme;
}

void imappWindowThemeFillDefault( ImappWindowTheme* windowTheme )
{
	const ImuiSkin skin = { IMUI_TEXTURE_HANDLE_INVALID };

	const ImuiColor brightColor					= imuiColorCreateGray( 192u );
	const ImuiColor mediumColor					= imuiColorCreateGray( 128u );
	const ImuiColor lowColor					= imuiColorCreateGray( 96u );

	windowTheme->titleActive.skin				= skin;
	windowTheme->titleActive.textColor			= imuiColorCreateBlack();
	windowTheme->titleActive.backgroundColor	= brightColor;
	windowTheme->titleInactive.skin				= skin;
	windowTheme->titleInactive.textColor		= imuiColorCreateBlack();
	windowTheme->titleInactive.backgroundColor	= mediumColor;

	const ImuiImage image = { IMUI_TEXTURE_HANDLE_INVALID };

	ImappWindowThemeTitleButton* buttons[] = {
		&windowTheme->titleMinimizeButton,
		&windowTheme->titleRestoreButton,
		&windowTheme->titleMaximizeButton,
		&windowTheme->titleCloseButton
	};

	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( buttons ); ++i )
	{
		ImappWindowThemeTitleButton* button = buttons[ i ];

		button->skin					= skin;
		button->size					= imuiSizeCreateAll( 30.0f );
		button->icon					= image;
		button->iconSize				= imuiSizeCreateZero();
		button->iconColor				= imuiColorCreateBlack();
		button->iconColorInactive		= imuiColorCreateBlack();
		button->iconColorHover			= imuiColorCreateBlack();
		button->iconColorClicked		= imuiColorCreateBlack();
		button->backgroundColor			= brightColor;
		button->backgroundColorInactive	= mediumColor;
		button->backgroundColorHover	= mediumColor;
		button->backgroundColorClicked	= lowColor;
	}

	windowTheme->titlePadding									= imuiBorderCreate( 0.0f, 5.0f, 0.0f, 0.0f );
	windowTheme->titleHeight									= 30.0f;
	windowTheme->titleSpacing									= 4.0f;

	windowTheme->bodyActive.skin								= skin;
	windowTheme->bodyActive.color								= imuiColorCreateWhite();
	windowTheme->bodyInactive.skin								= skin;
	windowTheme->bodyInactive.color								= brightColor;

	windowTheme->bodyPadding									= imuiBorderCreateAll( 5.0f );
}

bool imappWindowThemeTitleButtonUi( ImuiWindow* window, const ImappWindowThemeTitleButton* buttonTheme, bool hasFocus, const char* alternativeText )
{
	ImuiWidget* button = imuiWidgetBegin( window );
	imuiWidgetSetFixedSize( button, buttonTheme->size );

	ImuiWidgetInputState inputState;
	imuiWidgetGetInputState( button, &inputState );

	ImuiColor iconColor = hasFocus ? buttonTheme->iconColor : buttonTheme->iconColorInactive;
	ImuiColor backgroundColor = hasFocus ? buttonTheme->backgroundColor : buttonTheme->backgroundColorInactive;
	if( inputState.wasPressed && inputState.isMouseDown )
	{
		iconColor = buttonTheme->iconColorClicked;
		backgroundColor = buttonTheme->backgroundColorClicked;
	}
	else if( inputState.isMouseOver )
	{
		iconColor = buttonTheme->iconColorHover;
		backgroundColor = buttonTheme->backgroundColorHover;
	}

	imuiWidgetDrawSkin( button, &buttonTheme->skin, backgroundColor );

	if( buttonTheme->icon.textureHandle )
	{
		ImuiWidget* buttonIcon = imuiWidgetBegin( window );
		imuiWidgetSetAlign( buttonIcon, 0.5f, 0.5f );
		imuiWidgetSetFixedSize( buttonIcon, buttonTheme->iconSize );

		imuiWidgetDrawImageColor( buttonIcon, &buttonTheme->icon, iconColor );

		imuiWidgetEnd( buttonIcon );
	}
	else
	{
		ImuiWidget* buttonText = imuiToolboxLabelBeginColor( window, alternativeText, iconColor );
		imuiWidgetSetHAlign( buttonText, 0.5f );

		imuiWidgetEnd( buttonText );
	}

	imuiWidgetEnd( button );

	return inputState.wasPressed && inputState.hasMouseReleased;
}

ImuiRect imappWindowThemeDoUi( ImappWindow* appWindow, ImuiSurface* surface )
{
	const bool hasFocus = imappWindowHasFocus( appWindow );
	const bool isMaximized = imappPlatformWindowGetState( appWindow ) == ImappWindowState_Maximized;

	ImuiWindow* window = imuiWindowBegin( surface, "window", imuiSurfaceGetRect( surface ), 1 );

	ImuiWidget* root = imuiWidgetBegin( window );
	imuiWidgetSetStretchOne( root );
	imuiWidgetSetLayoutVertical( root );

	// title bar
	int buttonsX;
	int titleHeight;
	{
		ImuiWidget* title = imuiWidgetBegin( window );
		imuiWidgetSetHStretch( title, 1.0f );
		imuiWidgetSetLayoutHorizontalSpacing( title, s_windowTheme.titleSpacing );
		imuiWidgetSetFixedHeight( title, s_windowTheme.titleHeight );
		imuiWidgetSetPadding( title, s_windowTheme.titlePadding );

		const ImappWindowThemeTitle* titleTheme = hasFocus ? &s_windowTheme.titleActive : &s_windowTheme.titleInactive;
		imuiWidgetDrawSkin( title, &titleTheme->skin, titleTheme->backgroundColor );

		//imappWindowThemeState* state = (imappWindowThemeState*)imuiWidgetAllocState( title, sizeof( imappWindowThemeState ), 0u );

		ImuiWidgetInputState inputState;
		imuiWidgetGetInputState( title, &inputState );

		//if( inputState.hasMousePressed )
		//{
		//	state->clickPosition = inputState.relativeMousePos;
		//}

		//if( inputState.wasPressed )
		//{
		//	const imuiPos mouseMove = imuiPosSubPos( inputState.relativeMousePos, state->clickPosition );
		//	if( mouseMove.x != 0.0f || mouseMove.y != 0.0f )
		//	{
		//		int x;
		//		int y;
		//		imappWindowGetPosition( appWindow, &x, &y );
		//		x += (int)mouseMove.x;
		//		y += (int)mouseMove.y;

		//		imappWindowSetPosition( appWindow, x, y );
		//	}
		//}

		if( inputState.isMouseOver &&
			imuiInputHasMouseButtonDoubleClicked( imuiWidgetGetInput( title ), ImuiInputMouseButton_Left ) )
		{
			imappPlatformWindowSetState( appWindow, isMaximized ? ImappWindowState_Default : ImappWindowState_Maximized );
		}

		// title text
		{
			const char* windowTitle = imappPlatformWindowGetTitle( appWindow );

			ImuiWidget* titleText = imuiToolboxLabelBeginColor( window, windowTitle, titleTheme->textColor );
			imuiWidgetSetVAlign( titleText, 0.5f );
			imuiToolboxLabelEnd( titleText );
		}

		ImuiWidget* strecher = imuiWidgetBegin( window );
		imuiWidgetSetHStretch( strecher, 1.0f );
		{
			const ImuiRect strecherRect = imuiWidgetGetRect( strecher );
			buttonsX = (int)(strecherRect.pos.x + strecherRect.size.width);
		}
		imuiWidgetEnd( strecher );

		// minimize button
		{
			const ImappWindowThemeTitleButton* buttonTheme = &s_windowTheme.titleMinimizeButton;
			if( imappWindowThemeTitleButtonUi( window, buttonTheme, hasFocus, "_") )
			{
				imappPlatformWindowSetState( appWindow, ImappWindowState_Minimized );
			}
		}

		// maximize/restore button
		{
			const ImappWindowThemeTitleButton* buttonTheme = isMaximized ? &s_windowTheme.titleRestoreButton : &s_windowTheme.titleMaximizeButton;
			const char* alternativeText = isMaximized ? "[]" : "^";
			if( imappWindowThemeTitleButtonUi( window, buttonTheme, hasFocus, alternativeText ) )
			{
				imappPlatformWindowSetState( appWindow, isMaximized ? ImappWindowState_Default : ImappWindowState_Maximized );
			}
		}

		// close button
		{
			const ImappWindowThemeTitleButton* buttonTheme = &s_windowTheme.titleCloseButton;
			if( imappWindowThemeTitleButtonUi( window, buttonTheme, hasFocus, "X" ) )
			{
				imappPlatformWindowClose( appWindow );
			}
		}

		titleHeight = (int)imuiWidgetGetSizeHeight( title );

		imuiWidgetEnd( title );
	}

	// body
	ImuiRect result;
	{
		ImuiWidget* body = imuiWidgetBegin( window );
		imuiWidgetSetStretchOne( body );
		imuiWidgetSetPadding( body, s_windowTheme.bodyPadding );

		const ImappWindowThemeBody* bodyTheme = hasFocus ? &s_windowTheme.bodyActive : &s_windowTheme.bodyInactive;
		imuiWidgetDrawSkin( body, &bodyTheme->skin, bodyTheme->color );

		result = imuiRectShrinkBorder( imuiWidgetGetRect( body ), s_windowTheme.bodyPadding );

		imuiWidgetEnd( body );
	}

	imuiWidgetEnd( root );
	imuiWindowEnd( window );

	imappPlatformWindowSetTitleBounds( appWindow, titleHeight, buttonsX );

	return result;
}

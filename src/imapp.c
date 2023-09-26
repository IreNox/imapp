#include "imapp/imapp.h"

#include "imapp_resource_storage.h"

#include "imapp_internal.h"

void ImAppQuit( ImAppContext* imapp )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;

	pImApp->running = false;
}

ImAppImage* ImAppImageLoadResource( ImAppContext* imapp, ImUiStringView resourceName )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;

	ImAppImage* pImage = ImAppResourceStorageImageFindOrLoad( pImApp->resources, resourceName, false );
	if( pImage == NULL )
	{
		return NULL;
	}

	return pImage;
}

ImAppImage* ImAppImageLoadFromMemory( ImAppContext* imapp, const void* imageData, size_t imageDataSize, int width, int height )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;
	return ImAppResourceStorageImageCreateFromMemory( pImApp->resources, imageData, width, height );
}

void ImAppImageFree( ImAppContext* imapp, ImAppImage* image )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;
	ImAppResourceStorageImageFree( pImApp->resources, image );
}

ImUiTexture ImAppImageGetTexture( const ImAppImage* image )
{
	ImUiTexture texture;

	if( image )
	{
		texture.data	= image->pTexture;
		texture.size	= image->size;
	}
	else
	{
		texture.data	= NULL;
		texture.size	= ImUiSizeCreateZero();
	}

	return texture;
}

ImUiTexture ImAppImageGet( ImAppContext* imapp, ImUiStringView resourceName, int defaultWidth, int defaultHeight, ImUiColor defaultColor )
{
	// TODO: implement async loading
	return ImAppImageGetBlocking( imapp, resourceName );
}

ImUiTexture ImAppImageGetBlocking( ImAppContext* imapp, ImUiStringView resourceName )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;

	const ImAppImage* pImage = ImAppResourceStorageImageFindOrLoad( pImApp->resources, resourceName, false );
	return ImAppImageGetTexture( pImage );
}

ImUiFont* ImAppFontGet( ImAppContext* imapp, ImUiStringView fontName, float fontSize )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;

	return ImAppResourceStorageFontCreate( pImApp->resources, fontName, fontSize );
}

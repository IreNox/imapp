#include "imapp/imapp.h"

#include "imapp_image_storage.h"

#include "imapp_private.h"

void ImAppQuit( ImAppContext* imAppContext )
{
	ImApp* pImApp = (ImApp*)imAppContext;

	pImApp->running = false;
}

ImAppImage* ImAppImageLoadResource( ImAppContext* imAppContext, const char* resourceName )
{
	ImApp* pImApp = (ImApp*)imAppContext;

	ImAppImage* pImage = ImAppImageStorageFindImageOrLoad( pImApp->pImages, resourceName, false );
	if( pImage == NULL )
	{
		return NULL;
	}

	return pImage;
}

ImAppImage* ImAppImageLoadFromMemory( ImAppContext* imAppContext, const void* imageData, size_t imageDataSize, int width, int height )
{
	ImApp* pImApp = (ImApp*)imAppContext;
	return ImAppImageStorageCreateImageFromMemory( pImApp->pImages, imageData, width, height );
}

void ImAppImageFree( ImAppContext* imAppContext, ImAppImage* image )
{
	ImApp* pImApp = (ImApp*)imAppContext;
	ImAppImageStorageFreeImage( pImApp->pImages, image );
}

struct nk_image ImAppImageNuklear( ImAppImage* image )
{
	return nk_subimage_ptr( image->pTexture, image->width, image->height, nk_recti( 0, 0, image->width, image->height ) );
}

struct nk_image ImAppImageGet( ImAppContext* imAppContext, const char* resourceName, int defaultWidth, int defaultHeight, ImAppColor defaultColor )
{
	// TODO: implement async loading
	return ImAppImageGetBlocking( imAppContext, resourceName );
}

struct nk_image ImAppImageGetBlocking( ImAppContext* imAppContext, const char* resourceName )
{
	ImApp* pImApp = (ImApp*)imAppContext;

	const ImAppImage* pImage = ImAppImageStorageFindImageOrLoad( pImApp->pImages, resourceName, false );
	if( pImage == NULL )
	{
		return nk_image_ptr( NULL );
	}

	return nk_subimage_ptr( pImage->pTexture, pImage->width, pImage->height, nk_recti( 0, 0, pImage->width, pImage->height ) );
}

uint8_t ImAppColorGetR( ImAppColor color )
{
	return (color >> 24u) & 0xffu;
}

uint8_t ImAppColorGetG( ImAppColor color )
{
	return (color >> 16u) & 0xffu;
}

uint8_t ImAppColorGetB( ImAppColor color )
{
	return (color >> 8u) & 0xffu;
}

uint8_t ImAppColorGetA( ImAppColor color )
{
	return color & 0xffu;
}

ImAppColor ImAppColorSetRGB( uint8_t r, uint8_t g, uint8_t b )
{
	return ImAppColorSetRGBA( r, g, b, 0xffu );
}

ImAppColor ImAppColorSetRGBA( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
	return ((ImAppColor)r << 24u) |
		((ImAppColor)g << 16u) |
		((ImAppColor)b << 8u) |
		(ImAppColor)a;
}

ImAppColor ImAppColorSetFloatRGB( float r, float g, float b )
{
	return ImAppColorSetFloatRGBA( r, g, b, 1.0f );
}

ImAppColor ImAppColorSetFloatRGBA( float r, float g, float b, float a )
{
	const uint8_t ri = (uint8_t)((r * 255.0f) + 0.5f);
	const uint8_t gi = (uint8_t)((g * 255.0f) + 0.5f);
	const uint8_t bi = (uint8_t)((b * 255.0f) + 0.5f);
	const uint8_t ai = (uint8_t)((a * 255.0f) + 0.5f);
	return ImAppColorSetRGBA( ri, gi, bi, ai );
}

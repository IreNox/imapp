#include "imapp_image_storage.h"

#include "imapp_defines.h"
#include "imapp_platform.h"
#include "imapp_renderer.h"

#include <lodepng.h>
#include <stdlib.h>

enum ImAppImageState
{
	ImAppImageState_Invalid,
	ImAppImageState_Default,
	ImAppImageState_Managed,
	ImAppImageState_Unused
};
typedef enum ImAppImageState ImAppImageState;

struct ImAppImageInternal;
typedef struct ImAppImageInternal ImAppImageInternal;

struct ImAppImageNode
{
	ImAppImageInternal*		pPrev;
	ImAppImageInternal*		pNext;
};
typedef struct ImAppImageNode ImAppImageNode;

struct ImAppImageInternal
{
	ImAppImage				image;

	uint32_t				nameHash;
	ImAppImageState			state;

	ImAppImageNode			stateNode;
	ImAppImageNode			mapNode;
};

struct ImAppImageList
{
	ImAppImageInternal*		pFirst;
	ImAppImageInternal*		pLast;
};
typedef struct ImAppImageList ImAppImageList;

struct ImAppImageBucket
{
	ImAppImageList			images;
};
typedef struct ImAppImageBucket ImAppImageBucket;

struct ImAppImageStorage
{
	ImAppAllocator*			pAllocator;
	ImAppPlatform*			pPlatform;
	ImAppRenderer*			pRenderer;

	ImAppImageList			defaultImages;
	ImAppImageList			managedImages;
	ImAppImageList			unusedImages;

	ImAppImageBucket		map[ 256u ];
};

#define IMAPP_FNV1A_PRIME	0x01000193
#define IMAPP_FNV1A_SEED	0x811C9DC5

static void				ImAppImageStorageChangeState( ImAppImageStorage* pStorage, ImAppImageInternal* pImage, ImAppImageState state );
static void				ImAppImageStroageFreeImageInternal( ImAppImageStorage* pStorage, ImAppImageInternal* pImage );

static uint32_t			ImAppImageHashName( const char* pName );

static void				ImAppImageListAdd( ImAppImageList* pList, ImAppImageInternal* pImage, size_t nodeOffset );
static void				ImAppImageListRemove( ImAppImageList* pList, ImAppImageInternal* pImage, size_t nodeOffset );
static ImAppImageNode*	ImAppImageListGetNode( ImAppImageInternal* pImage, size_t nodeOffset );

ImAppImageStorage* ImAppImageStorageCreate( ImAppAllocator* pAllocator, ImAppPlatform* pPlatform, ImAppRenderer* pRenderer )
{
	ImAppImageStorage* pStroage = IMAPP_NEW_ZERO( pAllocator, ImAppImageStorage );

	pStroage->pAllocator	= pAllocator;
	pStroage->pPlatform		= pPlatform;
	pStroage->pRenderer		= pRenderer;

	return pStroage;
}

void ImAppImageStorageDestroy( ImAppImageStorage* pStorage )
{
	while( pStorage->defaultImages.pFirst )
	{
		ImAppImageStroageFreeImageInternal( pStorage, pStorage->defaultImages.pFirst );
	}

	while( pStorage->managedImages.pFirst )
	{
		ImAppImageStroageFreeImageInternal( pStorage, pStorage->managedImages.pFirst );
	}

	while( pStorage->unusedImages.pFirst )
	{
		ImAppImageStroageFreeImageInternal( pStorage, pStorage->unusedImages.pFirst );
	}

	ImAppFree( pStorage->pAllocator, pStorage );
}

void ImAppImageStorageUpdate( ImAppImageStorage* pStorage )
{
	while( pStorage->unusedImages.pFirst )
	{
		ImAppImageStroageFreeImageInternal( pStorage, pStorage->unusedImages.pFirst );
	}

	pStorage->unusedImages = pStorage->managedImages;
	IMAPP_ZERO( pStorage->managedImages );

	for( ImAppImageInternal* pImage = pStorage->unusedImages.pFirst; pImage != NULL; pImage = pImage->stateNode.pNext )
	{
		pImage->state = ImAppImageState_Unused;
	}
}

ImAppImage* ImAppImageStorageFindImageOrLoad( ImAppImageStorage* pStorage, const char* pResourceName, bool autoFree )
{
	const uint32_t nameHash = ImAppImageHashName( pResourceName );

	ImAppImageBucket* pMapBucket = &pStorage->map[ nameHash & 0xffu ];
	for( ImAppImageInternal* pImage = pMapBucket->images.pFirst; pImage != NULL; pImage = pImage->mapNode.pNext )
	{
		if( pImage->nameHash != nameHash )
		{
			continue;
		}

		if( pImage->state != ImAppImageState_Default )
		{
			ImAppImageStorageChangeState( pStorage, pImage, autoFree ? ImAppImageState_Managed : ImAppImageState_Default );
		}

		if( pImage->state == ImAppImageState_Unused )
		{
			const size_t nodeOffset = IMAPP_OFFSETOF( ImAppImageInternal, stateNode );
			ImAppImageListRemove( &pStorage->unusedImages, pImage, nodeOffset );
			ImAppImageListAdd( &pStorage->managedImages, pImage, nodeOffset );

			pImage->state = ImAppImageState_Managed;
		}

		return &pImage->image;
	}

	const ImAppResource imageResource = ImAppResourceLoad( pStorage->pPlatform, pStorage->pAllocator, pResourceName );
	if( imageResource.pData == NULL )
	{
		return NULL;
	}

	uint8_t* pPixelData;
	unsigned width;
	unsigned height;
	const unsigned pngResult = lodepng_decode_memory( &pPixelData, &width, &height, (const unsigned char*)imageResource.pData, imageResource.size, LCT_RGBA, 8u );
	ImAppFree( pStorage->pAllocator, (void*)imageResource.pData );

	if( pngResult != 0 )
	{
		return NULL;
	}

	ImAppRendererTexture* pTexture = ImAppRendererTextureCreateFromMemory( pStorage->pRenderer, pPixelData, width, height );
	free( pPixelData );

	if( pTexture == NULL )
	{
		return NULL;
	}

	ImAppImageInternal* pImage = IMAPP_NEW_ZERO( pStorage->pAllocator, ImAppImageInternal );
	pImage->image.pTexture	= pTexture;
	pImage->image.width		= width;
	pImage->image.height	= height;
	pImage->nameHash		= nameHash;

	ImAppImageStorageChangeState( pStorage, pImage, autoFree ? ImAppImageState_Managed : ImAppImageState_Default );

	ImAppImageListAdd( &pMapBucket->images, pImage, IMAPP_OFFSETOF( ImAppImageInternal, mapNode ) );

	return &pImage->image;
}

ImAppImage* ImAppImageStorageCreateImageFromMemory( ImAppImageStorage* pStorage, const void* pPixelData, int width, int height )
{
	ImAppRendererTexture* pTexture = ImAppRendererTextureCreateFromMemory( pStorage->pRenderer, pPixelData, width, height );
	if( pTexture == NULL )
	{
		return NULL;
	}

	ImAppImageInternal* pImage = IMAPP_NEW_ZERO( pStorage->pAllocator, ImAppImageInternal );
	pImage->image.pTexture	= pTexture;
	pImage->image.width		= width;
	pImage->image.height	= height;

	ImAppImageStorageChangeState( pStorage, pImage, ImAppImageState_Default );

	return &pImage->image;
}

void ImAppImageStorageFreeImage( ImAppImageStorage* pStorage, ImAppImage* pImage )
{
	ImAppImageInternal* pInternalImage = (ImAppImageInternal*)pImage;
	ImAppImageStroageFreeImageInternal( pStorage, pInternalImage );
}

static void ImAppImageStroageFreeImageInternal( ImAppImageStorage* pStorage, ImAppImageInternal* pImage )
{
	ImAppImageStorageChangeState( pStorage, pImage, ImAppImageState_Invalid );

	if( pImage->nameHash != 0u )
	{
		ImAppImageBucket* pMapBucket = &pStorage->map[ pImage->nameHash & 0xffu ];
		ImAppImageListRemove( &pMapBucket->images, pImage, IMAPP_OFFSETOF( ImAppImageInternal, mapNode ) );
	}

	ImAppRendererTextureDestroy( pStorage->pRenderer, pImage->image.pTexture );
	pImage->image.pTexture = NULL;

	ImAppFree( pStorage->pAllocator, pImage );
}

static uint32_t ImAppImageHashName( const char* pName )
{
	uint32_t hash = IMAPP_FNV1A_SEED;
	while( *pName != '\0' )
	{
		hash = (*pName ^ hash) * IMAPP_FNV1A_PRIME;
		pName++;
	}
	return hash;
}

static void ImAppImageStorageChangeState( ImAppImageStorage* pStorage, ImAppImageInternal* pImage, ImAppImageState state )
{
	if( pImage->state == state )
	{
		return;
	}

	const size_t nodeOffset = IMAPP_OFFSETOF( ImAppImageInternal, stateNode );

	ImAppImageList* pOldList = NULL;
	switch( pImage->state )
	{
	case ImAppImageState_Invalid:
		break;

	case ImAppImageState_Default:
		pOldList = &pStorage->defaultImages;
		break;

	case ImAppImageState_Managed:
		pOldList = &pStorage->managedImages;
		break;

	case ImAppImageState_Unused:
		pOldList = &pStorage->unusedImages;
		break;

	default:
		break;
	}

	if( pOldList != NULL )
	{
		ImAppImageListRemove( pOldList, pImage, nodeOffset );
	}

	ImAppImageList* pNewList = NULL;
	switch( state )
	{
	case ImAppImageState_Invalid:
		break;

	case ImAppImageState_Default:
		pNewList = &pStorage->defaultImages;
		break;

	case ImAppImageState_Managed:
		pNewList = &pStorage->managedImages;
		break;

	case ImAppImageState_Unused:
		pNewList = &pStorage->unusedImages;
		break;

	default:
		break;
	}

	if( pNewList != NULL )
	{
		ImAppImageListAdd( pNewList, pImage, nodeOffset );
	}

	pImage->state = state;
}

static void ImAppImageListAdd( ImAppImageList* pList, ImAppImageInternal* pImage, size_t nodeOffset )
{
	ImAppImageNode* pNode = ImAppImageListGetNode( pImage, nodeOffset );
	IMAPP_ASSERT( pNode->pPrev == NULL );
	IMAPP_ASSERT( pNode->pNext == NULL );

	if( pList->pFirst == NULL )
	{
		pList->pFirst = pImage;
	}

	if( pList->pLast == NULL )
	{
		pList->pLast = pImage;
		return;
	}

	ImAppImageNode* pLastNode = ImAppImageListGetNode( pImage, nodeOffset );
	IMAPP_ASSERT( pLastNode->pNext == NULL );
	pLastNode->pNext = pImage;
	pNode->pPrev = pList->pLast;

	pList->pLast = pImage;
}

static void ImAppImageListRemove( ImAppImageList* pList, ImAppImageInternal* pImage, size_t nodeOffset )
{
	ImAppImageNode* pNode = ImAppImageListGetNode( pImage, nodeOffset );

	if( pList->pFirst == pImage )
	{
		pList->pFirst = pNode->pNext;
	}

	if( pList->pLast == pImage )
	{
		pList->pLast = pNode->pPrev;
	}

	if( pNode->pPrev != NULL )
	{
		ImAppImageNode* pPrevNode = ImAppImageListGetNode( pNode->pPrev, nodeOffset );
		pPrevNode->pNext = pNode->pNext;
	}

	if( pNode->pNext != NULL )
	{
		ImAppImageNode* pNextNode = ImAppImageListGetNode( pNode->pNext, nodeOffset );
		pNextNode->pPrev = pNode->pPrev;
	}

	pNode->pPrev = NULL;
	pNode->pNext = NULL;
}

static ImAppImageNode* ImAppImageListGetNode( ImAppImageInternal* pImage, size_t nodeOffset )
{
	return (ImAppImageNode*)((size_t)pImage + nodeOffset);
}

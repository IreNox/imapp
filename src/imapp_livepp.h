#pragma once

#include "imapp_types.h"

#if IMAPP_ENABLED( IMAPP_LIVEPP )

bool	imappLivePlusPlusEnable( const wchar_t* basePath );
void	imappLivePlusPlusUpdate();
void	imappLivePlusPlusForceHotReload();

#endif

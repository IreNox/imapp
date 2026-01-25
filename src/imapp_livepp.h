#pragma once

#include "imapp_defines.h"

#if IMAPP_ENABLED( IMAPP_LIVEPP )

bool	imappLivePlusPlusEnable( const wchar_t* basePath );
void	imappLivePlusPlusUpdate();
void	imappLivePlusPlusForceHotReload();

#endif

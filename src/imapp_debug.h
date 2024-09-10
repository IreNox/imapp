#pragma once

#include "imapp_defines.h"

#if IMAPP_ENABLED( IMAPP_DEBUG )
#	define IMAPP_DEBUG_LOGI( fmt, ... )	ImAppTrace( "Info: " fmt "\n", ##__VA_ARGS__ )
#	define IMAPP_DEBUG_LOGW( fmt, ... )	ImAppTrace( "Warning: " fmt "\n", ##__VA_ARGS__ )
#	define IMAPP_DEBUG_LOGE( fmt, ... )	ImAppTrace( "Error: " fmt "\n", ##__VA_ARGS__ )
#else
#	define IMAPP_DEBUG_LOGI( fmt, ... )
#	define IMAPP_DEBUG_LOGW( fmt, ... )
#	define IMAPP_DEBUG_LOGE( fmt, ... )
#endif

#include "imapp_livepp.h"

#if IMAPP_ENABLED( IMAPP_LIVEPP )

#include "imapp_debug.h"

#include <LivePP/API/x64/LPP_API_x64_CPP.h>

static bool s_livePlusPlusAgentEnabled = false;
static LppSynchronizedAgent s_livePlusPlusAgent;

static void imappLivePlusPlusConnectionCallback( void* /*context*/, LppConnectionStatus status );

bool imappLivePlusPlusEnable( const wchar_t* basePath )
{
	s_livePlusPlusAgent = LppCreateSynchronizedAgent( NULL, basePath );
	if( !LppIsValidSynchronizedAgent( &s_livePlusPlusAgent ) )
	{
		return false;
	}

	s_livePlusPlusAgent.EnableModule( LppGetCurrentModulePath(), LPP_MODULES_OPTION_NONE, NULL, NULL );

	s_livePlusPlusAgent.OnConnection( NULL, &imappLivePlusPlusConnectionCallback );

	s_livePlusPlusAgentEnabled = true;
	return true;
}

void imappLivePlusPlusUpdate()
{
	if( !s_livePlusPlusAgentEnabled )
	{
		return;
	}

	if( s_livePlusPlusAgent.WantsReload( LPP_RELOAD_OPTION_SYNCHRONIZE_WITH_RELOAD ) )
	{
		s_livePlusPlusAgent.Reload( LPP_RELOAD_BEHAVIOUR_WAIT_UNTIL_CHANGES_ARE_APPLIED );
	}

	if( s_livePlusPlusAgent.WantsRestart() )
	{
		s_livePlusPlusAgent.Restart( LPP_RESTART_BEHAVIOUR_INSTANT_TERMINATION, 0u, NULL );
	}
}

void imappLivePlusPlusForceHotReload()
{
	if( !s_livePlusPlusAgentEnabled )
	{
		return;
	}

	s_livePlusPlusAgent.Reload( LPP_RELOAD_BEHAVIOUR_WAIT_UNTIL_CHANGES_ARE_APPLIED );
}

static void imappLivePlusPlusConnectionCallback( void* context, LppConnectionStatus status )
{
	if( status == LPP_CONNECTION_STATUS_SUCCESS )
	{
		IMAPP_DEBUG_LOGI( "Live++ connection succesful" );
	}
	else if( status == LPP_CONNECTION_STATUS_FAILURE )
	{
		IMAPP_DEBUG_LOGI( "Live++ connection failed" );
	}
	else if( status == LPP_CONNECTION_STATUS_UNEXPECTED_VERSION_BLOB ||
		status == LPP_CONNECTION_STATUS_VERSION_MISMATCH )
	{
		IMAPP_DEBUG_LOGI( "Live++ has a incompatible agent and broker" );
	}
}

#endif

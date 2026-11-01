//========= GoldenEye: Source ================================================//
//
// miniaudio-backed implementation of the FMOD subset in ge_fmod_compat.h.
// Compiled without the client precompiled header (miniaudio must not see
// the engine's memory/string macros).
//
//============================================================================//
#ifndef _WIN32 // Windows uses the real FMOD

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include "miniaudio.h"

#include "ge_fmod_compat.h"
#include <stdlib.h>
#include <string.h>

namespace FMOD
{

FMOD_RESULT System_Create( System **ppSystem )
{
	System *pSys = new System;
	memset( pSys, 0, sizeof( *pSys ) );
	*ppSystem = pSys;
	return FMOD_OK;
}

FMOD_RESULT System::init( int, int, void * )
{
	ma_engine *pEngine = (ma_engine *)calloc( 1, sizeof( ma_engine ) );
	ma_engine_config config = ma_engine_config_init();
	if ( ma_engine_init( &config, pEngine ) != MA_SUCCESS )
	{
		free( pEngine );
		return FMOD_ERR_INTERNAL;
	}
	m_pEngine = pEngine;

	ma_sound_group *pGroup = (ma_sound_group *)calloc( 1, sizeof( ma_sound_group ) );
	if ( ma_sound_group_init( pEngine, 0, NULL, pGroup ) != MA_SUCCESS )
	{
		free( pGroup );
		ma_engine_uninit( pEngine );
		free( pEngine );
		m_pEngine = NULL;
		return FMOD_ERR_INTERNAL;
	}
	m_Master.m_pGroup = pGroup;
	m_Master.m_flVolume = 1.0f;
	m_Master.m_bMute = false;
	m_Master.m_pSystem = this;
	return FMOD_OK;
}

FMOD_RESULT System::getMasterChannelGroup( ChannelGroup **ppGroup )
{
	*ppGroup = m_pEngine ? &m_Master : NULL;
	return m_pEngine ? FMOD_OK : FMOD_ERR_INTERNAL;
}

FMOD_RESULT System::createStream( const char *name, int, void *, Sound **ppSound )
{
	*ppSound = NULL;
	if ( !m_pEngine )
		return FMOD_ERR_INTERNAL;

	ma_sound *pMaSound = (ma_sound *)calloc( 1, sizeof( ma_sound ) );
	ma_result r = ma_sound_init_from_file( (ma_engine *)m_pEngine, name, MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
		(ma_sound_group *)m_Master.m_pGroup, NULL, pMaSound );
	if ( r != MA_SUCCESS )
	{
		free( pMaSound );
		return r == MA_DOES_NOT_EXIST ? FMOD_ERR_FILE_NOTFOUND : FMOD_ERR_INTERNAL;
	}

	Sound *pSound = new Sound;
	pSound->m_pSound = pMaSound;
	pSound->m_pszPath = strdup( name );
	pSound->m_pChannel = NULL;
	*ppSound = pSound;
	return FMOD_OK;
}

FMOD_RESULT System::playSound( Sound *pSound, ChannelGroup *, bool paused, Channel **ppChannel )
{
	if ( !pSound )
		return FMOD_ERR_INTERNAL;

	Channel *pChannel = new Channel;
	pChannel->m_pSound = pSound;
	pSound->m_pChannel = pChannel;
	if ( !paused )
		ma_sound_start( (ma_sound *)pSound->m_pSound );
	*ppChannel = pChannel;
	return FMOD_OK;
}

FMOD_RESULT System::update()
{
	// miniaudio mixes on its own device thread
	return FMOD_OK;
}

FMOD_RESULT System::release()
{
	if ( m_Master.m_pGroup )
	{
		ma_sound_group_uninit( (ma_sound_group *)m_Master.m_pGroup );
		free( m_Master.m_pGroup );
	}
	if ( m_pEngine )
	{
		ma_engine_uninit( (ma_engine *)m_pEngine );
		free( m_pEngine );
	}
	delete this;
	return FMOD_OK;
}

// ---------------------------------------------------------------------------

static void ApplyGroupVolume( ChannelGroup *pGroup )
{
	if ( pGroup->m_pGroup )
		ma_sound_group_set_volume( (ma_sound_group *)pGroup->m_pGroup, pGroup->m_bMute ? 0.0f : pGroup->m_flVolume );
}

FMOD_RESULT ChannelGroup::setVolume( float volume )
{
	m_flVolume = volume;
	ApplyGroupVolume( this );
	return FMOD_OK;
}

FMOD_RESULT ChannelGroup::setMute( bool mute )
{
	m_bMute = mute;
	ApplyGroupVolume( this );
	return FMOD_OK;
}

FMOD_RESULT ChannelGroup::stop()
{
	// Songs are stopped/released individually by the music manager
	return FMOD_OK;
}

// ---------------------------------------------------------------------------

FMOD_RESULT Sound::getLength( unsigned int *length, int )
{
	float flLen = 0;
	ma_sound_get_length_in_seconds( (ma_sound *)m_pSound, &flLen );

	if ( flLen <= 0 && m_pszPath )
	{
		// Some streamed MP3s don't report a length up front; scan the file once.
		ma_decoder decoder;
		if ( ma_decoder_init_file( m_pszPath, NULL, &decoder ) == MA_SUCCESS )
		{
			ma_uint64 frames = 0;
			if ( ma_decoder_get_length_in_pcm_frames( &decoder, &frames ) == MA_SUCCESS && decoder.outputSampleRate )
				flLen = (float)frames / (float)decoder.outputSampleRate;
			ma_decoder_uninit( &decoder );
		}
	}

	*length = (unsigned int)( flLen * 1000.0f );
	return FMOD_OK;
}

FMOD_RESULT Sound::release()
{
	if ( m_pSound )
	{
		ma_sound_stop( (ma_sound *)m_pSound );
		ma_sound_uninit( (ma_sound *)m_pSound );
		free( m_pSound );
	}
	free( m_pszPath );
	if ( m_pChannel )
		delete m_pChannel;
	delete this;
	return FMOD_OK;
}

// ---------------------------------------------------------------------------

FMOD_RESULT Channel::setVolume( float volume )
{
	ma_sound_set_volume( (ma_sound *)m_pSound->m_pSound, volume );
	return FMOD_OK;
}

FMOD_RESULT Channel::setPaused( bool paused )
{
	if ( paused )
		ma_sound_stop( (ma_sound *)m_pSound->m_pSound );
	else
		ma_sound_start( (ma_sound *)m_pSound->m_pSound );
	return FMOD_OK;
}

FMOD_RESULT Channel::getPosition( unsigned int *position, int )
{
	float flCursor = 0;
	ma_sound_get_cursor_in_seconds( (ma_sound *)m_pSound->m_pSound, &flCursor );
	*position = (unsigned int)( flCursor * 1000.0f );
	return FMOD_OK;
}

FMOD_RESULT Channel::getCurrentSound( Sound **ppSound )
{
	*ppSound = m_pSound;
	return m_pSound ? FMOD_OK : FMOD_ERR_INTERNAL;
}

FMOD_RESULT Channel::stop()
{
	ma_sound_stop( (ma_sound *)m_pSound->m_pSound );
	return FMOD_OK;
}

} // namespace FMOD

#endif // !_WIN32

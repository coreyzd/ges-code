//========= GoldenEye: Source ================================================//
//
// Minimal FMOD Ex API subset backed by miniaudio, so the music manager works on
// platforms where we don't ship FMOD (Linux). Only what CGEMusicManager uses.
//
//============================================================================//
#ifndef GE_FMOD_COMPAT_H
#define GE_FMOD_COMPAT_H
#pragma once

typedef int FMOD_RESULT;
#define FMOD_OK					0
#define FMOD_ERR_INTERNAL		1
#define FMOD_ERR_FILE_NOTFOUND	2
#define FMOD_DEFAULT			0
#define FMOD_INIT_NORMAL		0
#define FMOD_TIMEUNIT_MS		1

namespace FMOD
{
	class System;
	class Sound;
	class Channel;
	class ChannelGroup;

	FMOD_RESULT System_Create( System **ppSystem );

	class ChannelGroup
	{
	public:
		FMOD_RESULT setVolume( float volume );
		FMOD_RESULT setMute( bool mute );
		FMOD_RESULT stop();

		void *m_pGroup;		// ma_sound_group*
		float m_flVolume;
		bool m_bMute;
		System *m_pSystem;
	};

	class Sound
	{
	public:
		FMOD_RESULT getLength( unsigned int *length, int timeunit );
		FMOD_RESULT release();

		void *m_pSound;		// ma_sound*
		char m_szPath[1024];
		Channel *m_pChannel;
	};

	class Channel
	{
	public:
		FMOD_RESULT setVolume( float volume );
		FMOD_RESULT setPaused( bool paused );
		FMOD_RESULT getPosition( unsigned int *position, int timeunit );
		FMOD_RESULT getCurrentSound( Sound **ppSound );
		FMOD_RESULT stop();

		Sound *m_pSound;
	};

	class System
	{
	public:
		FMOD_RESULT init( int maxchannels, int flags, void *extradriverdata );
		FMOD_RESULT getMasterChannelGroup( ChannelGroup **ppGroup );
		FMOD_RESULT createStream( const char *name, int mode, void *exinfo, Sound **ppSound );
		FMOD_RESULT playSound( Sound *pSound, ChannelGroup *pGroup, bool paused, Channel **ppChannel );
		FMOD_RESULT update();
		FMOD_RESULT release();

		void *m_pEngine;	// ma_engine*
		ChannelGroup m_Master;
	};
}

#endif // GE_FMOD_COMPAT_H

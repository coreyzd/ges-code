///////////// Copyright © 2011, Goldeneye: Source. All rights reserved. /////////////
// 
// File: ge_webrequest.h
// Description:
//      Initializes a web request in an asynchronous thread calling a defined
//      callback when the load is finished for processing
//
// Created On: 09 Oct 2011
// Created By: Jonathan White <killermonkey> 
/////////////////////////////////////////////////////////////////////////////

#include "cbase.h"
#include "ge_webrequest.h"

#ifdef _WIN32
#pragma warning( disable : 4005 )
#include "curl.h"
#pragma warning( default : 4005 )
#else
// On Linux we don't link libcurl; the Steam Runtime ships it, so load it at runtime.
#include <dlfcn.h>
typedef void CURL;
typedef int CURLcode;
#define CURLE_OK					0
#define CURLE_HTTP_RETURNED_ERROR	22
#define CURL_ERROR_SIZE				256
#define CURLOPT_WRITEDATA			10001
#define CURLOPT_URL					10002
#define CURLOPT_ERRORBUFFER			10010
#define CURLOPT_WRITEFUNCTION		20011
#define CURLOPT_FAILONERROR			45
#define CURLOPT_FOLLOWLOCATION		52
#define CURLOPT_TIMEOUT				13
#define CURLOPT_NOSIGNAL			99

static struct GECurlAPI
{
	bool bTried;
	void *hLib;
	CURL *(*easy_init)( void );
	CURLcode (*easy_setopt)( CURL *, int, ... );
	CURLcode (*easy_perform)( CURL * );
	void (*easy_cleanup)( CURL * );
	const char *(*easy_strerror)( CURLcode );

	bool Load()
	{
		if ( bTried )
			return hLib != NULL;
		bTried = true;
		const char *libs[] = { "libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so" };
		for ( int i = 0; i < 3 && !hLib; i++ )
			hLib = dlopen( libs[i], RTLD_NOW | RTLD_LOCAL );
		if ( !hLib )
			return false;
		*(void**)&easy_init		= dlsym( hLib, "curl_easy_init" );
		*(void**)&easy_setopt	= dlsym( hLib, "curl_easy_setopt" );
		*(void**)&easy_perform	= dlsym( hLib, "curl_easy_perform" );
		*(void**)&easy_cleanup	= dlsym( hLib, "curl_easy_cleanup" );
		*(void**)&easy_strerror	= dlsym( hLib, "curl_easy_strerror" );
		if ( !easy_init || !easy_setopt || !easy_perform || !easy_cleanup || !easy_strerror )
		{
			dlclose( hLib );
			hLib = NULL;
		}
		return hLib != NULL;
	}
} g_GECurl;

#define curl_easy_init			g_GECurl.easy_init
#define curl_easy_setopt		g_GECurl.easy_setopt
#define curl_easy_perform		g_GECurl.easy_perform
#define curl_easy_cleanup		g_GECurl.easy_cleanup
#define curl_easy_strerror		g_GECurl.easy_strerror
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CGEWebRequest::CGEWebRequest( const char *addr, WEBRQST_CLBK callback /*=NULL*/ )
{	
	m_pAddress = new char[Q_strlen(addr)+1];
	Q_strcpy( m_pAddress, addr );

	m_pError = new char[CURL_ERROR_SIZE+1];
	m_pError[0] = '\0';

	m_bFinished = false;

	m_pCallback = callback;

	// Start the thread
	SetName( "GEWebRequest" );
	Start();
}

CGEWebRequest::~CGEWebRequest()
{
	// Stop the thread
	if ( IsAlive() )
	{
		Terminate();
		Join();
	}
	
	// Clear the buffers
	delete [] m_pAddress;
	delete [] m_pError;
	m_Result.Purge();
}

void CGEWebRequest::Destroy()
{
	delete this;
}

size_t CGEWebRequest::WriteData( void *ptr, size_t size, size_t nmemb, void *userdata )
{
	CUtlBuffer *result = (CUtlBuffer*) userdata;
	// Data from curl is not NUL-terminated; keep the buffer terminated for GetResult()
	if ( result->TellPut() > 0 && ((char*)result->Base())[result->TellPut()-1] == '\0' )
		result->SeekPut( CUtlBuffer::SEEK_CURRENT, -1 );
	result->Put( ptr, size * nmemb );
	result->PutChar( '\0' );
	return size * nmemb;
}

int CGEWebRequest::Run()
{
	CURL *curl = NULL;
	CURLcode res;

#ifndef _WIN32
	if ( !g_GECurl.Load() )
	{
		Q_strncpy( m_pError, "libcurl not available", CURL_ERROR_SIZE );
		m_bFinished = true;
		return 0;
	}
#endif

	curl = curl_easy_init();
	if ( curl )
	{
		curl_easy_setopt( curl, CURLOPT_URL, m_pAddress );
		curl_easy_setopt( curl, CURLOPT_WRITEDATA, &m_Result );
		curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, &CGEWebRequest::WriteData );
		curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, m_pError );
		curl_easy_setopt( curl, CURLOPT_FAILONERROR, 1L );
		curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L );
		curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1L );
		curl_easy_setopt( curl, CURLOPT_TIMEOUT, 15L );

		res = curl_easy_perform( curl );
		if ( res != CURLE_OK && res != CURLE_HTTP_RETURNED_ERROR )
			Q_strncpy( m_pError, curl_easy_strerror( res ), CURL_ERROR_SIZE );

		curl_easy_cleanup( curl );
	}
	else
	{
		Q_strncpy( m_pError, "Failed to start CURL", CURL_ERROR_SIZE );
	}

	m_bFinished = true;
	return 0;
}

void CGEWebRequest::OnExit()
{
	if ( m_pCallback )
		m_pCallback( (char*) m_Result.Base(), m_pError );
}
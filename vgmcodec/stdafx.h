// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define VC_EXTRALEAN
#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NO_SHLWAPI_STRFCNS
#define STRICT_CONST
#define _SDL_BANNED_RECOMMENDED
#ifndef STRICT
#define STRICT
#endif

#include "targetver.h"

#define _ATL_FREE_THREADED
#define _ATL_EX_CONVERSION_MACROS_ONLY
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_NO_WIN_SUPPORT

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit


#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <mfapi.h>
#include <Mfidl.h>
#include <Mferror.h>
#include <vector>
#include <queue>
#include <memory>
#include <array>
#include <cstdint>
#include <comdef.h>
#include <sstream>
#define BOOST_SCOPE_EXIT_CONFIG_USE_LAMBDAS
#define BOOST_IOSTREAMS_NO_LIB
#include <zlib.h>
#include <boost/scope_exit.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <comdef.h>
#include <cassert>
#include <strsafe.h>
#include "banned.h"
#define VGM_SAMPLE_RATE 44100U

template <class T>
static void ClearQueue(std::queue<T> &q)
{
	std::queue<T> empty;
	std::swap(q, empty);
}
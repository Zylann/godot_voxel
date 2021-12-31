#pragma once

#if !defined( FASTNOISE_STATIC_LIB ) && ( defined( _WIN32 ) || defined( __CYGWIN__ ) )
#ifdef FASTNOISE_EXPORT // CHANGE ME
#define FASTSIMD_API __declspec( dllexport )
#else
#define FASTSIMD_API __declspec( dllimport )
#endif
#else
#define FASTSIMD_API
#endif
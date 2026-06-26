//=============================================================================
// Linux port of win32/stacktrace.cpp: return addresses of the current call stack (glibc backtrace).
//=============================================================================

#include "pch.hpp"
#include <execinfo.h>
#include <string.h>
#include <radstacktrace.hpp>

extern "C" void radStackTraceGet( unsigned int* results, int max )
{
    void* frames[ 64 ];
    int n = backtrace( frames, max + 1 < 64 ? max + 1 : 64 );
    memset( results, 0, sizeof( unsigned int ) * max );
    for ( int i = 1; i < n && i - 1 < max; i++ )        // skip this function's own frame
    {
        results[ i - 1 ] = (unsigned int)(uintptr_t) frames[ i ];
    }
}

extern "C" void radStackTraceWin32Get( unsigned int* results, int numResults, const void* basePointer )
{
    radStackTraceGet( results, numResults );
}

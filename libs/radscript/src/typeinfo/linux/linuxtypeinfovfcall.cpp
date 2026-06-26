//=============================================================================
// Linux port of win32/win32typeinfovfcall.cpp: call virtual method number `vtbloffset` of pThat with
// `numParams` 32-bit parameters taken from pParams.
//
// MSVC's thiscall passes `this` in ECX (the original was inline asm). With GCC on i386 a virtual method
// is an ordinary cdecl function taking `this` as its first stack argument, so it can be called through
// a function pointer. Every parameter is one 32-bit stack slot (ints, pointers and floats alike), exactly
// as the original pushed them. NOTE: vtbloffset comes from srrtypes.typ, generated against MSVC's vtable
// layout; it matches GCC's as long as the scripted interfaces have no virtual destructors or overloaded
// virtual methods (the two places the ABIs order slots differently).
//=============================================================================

#include "pch.hpp"
#include <raddebug.hpp>

typedef unsigned int W;   // one 32-bit stack slot

int InvokeVf( void * pThat, unsigned int vtbloffset, void * pParams, int numParams )
{
    void** vtable = *(void***) pThat;
    void*  fn = vtable[ vtbloffset ];
    W*     p = (W*) pParams;

    switch ( numParams )
    {
    case 0: return ((int(*)(void*))fn)( pThat );
    case 1: return ((int(*)(void*,W))fn)( pThat, p[0] );
    case 2: return ((int(*)(void*,W,W))fn)( pThat, p[0], p[1] );
    case 3: return ((int(*)(void*,W,W,W))fn)( pThat, p[0], p[1], p[2] );
    case 4: return ((int(*)(void*,W,W,W,W))fn)( pThat, p[0], p[1], p[2], p[3] );
    case 5: return ((int(*)(void*,W,W,W,W,W))fn)( pThat, p[0], p[1], p[2], p[3], p[4] );
    case 6: return ((int(*)(void*,W,W,W,W,W,W))fn)( pThat, p[0], p[1], p[2], p[3], p[4], p[5] );
    case 7: return ((int(*)(void*,W,W,W,W,W,W,W))fn)( pThat, p[0], p[1], p[2], p[3], p[4], p[5], p[6] );
    case 8: return ((int(*)(void*,W,W,W,W,W,W,W,W))fn)( pThat, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7] );
    case 9: return ((int(*)(void*,W,W,W,W,W,W,W,W,W))fn)( pThat, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8] );
    case 10: return ((int(*)(void*,W,W,W,W,W,W,W,W,W,W))fn)( pThat, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9] );
    default:
        rAssertMsg( false, "InvokeVf: more than 10 script parameters" );
        return 0;
    }
}

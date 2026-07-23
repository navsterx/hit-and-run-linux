//===========================================================================
// Copyright (C) 2003 Radical Entertainment Ltd.  All rights reserved.
//
// Component:   Scrooby Text Bible Public Interface
//
// Authors:     Tony Chu
//
//===========================================================================

// Recompilation protection flag.
#ifndef SCROOBY_TEXTBIBLE_H
#define SCROOBY_TEXTBIBLE_H

#include <strings/unicodeString.h>

namespace Scrooby
{
#ifdef RAD_LINUX
// Linux port: consulted before every string lookup (hash of the string's name under the language's modulo); a
// non-NULL result replaces or adds the string (Mod Launcher CustomText, code/presentation/gui/guitextbible.cpp)
extern UnicodeChar* (*g_TextOverride)( unsigned int hash, unsigned int modulo );
#endif


//===========================================================================
// Interface Definitions
//===========================================================================

class TextBible
{
public:
    virtual UnicodeChar* GetWChar( unsigned int hash ) = 0;
    virtual UnicodeChar* GetWChar( const char* key ) = 0;

};

} // End namespace

#endif // End conditional inclusion

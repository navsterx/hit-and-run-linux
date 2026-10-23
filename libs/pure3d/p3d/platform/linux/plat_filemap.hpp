//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

// Linux port of p3d/platform/win32/plat_filemap.hpp: memory-mapped read-only file (mmap).

#ifndef _PLAT_FILEMAP_HPP
#define _PLAT_FILEMAP_HPP

#include <p3d/file.hpp>

class tLinuxFileMap : public tFileMem
{
public:
    tLinuxFileMap(const char* filename);

    bool IsOpen(void) { return GetMemory() != NULL; }

protected:
    virtual ~tLinuxFileMap();

    void Open(const char* filename);
    void Close(void);

    void*    mapped;
    unsigned mappedLength;
};

#endif /*_PLAT_FILEMAP_HPP*/

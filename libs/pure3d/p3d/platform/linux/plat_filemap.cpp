//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

// Linux port of p3d/platform/win32/plat_filemap.cpp: mmap() replaces CreateFileMapping/MapViewOfFile.

#include <p3d/platform/linux/plat_filemap.hpp>
#include <p3d/utility.hpp>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

tLinuxFileMap::tLinuxFileMap(const char* filename)
{
    mapped = NULL;
    mappedLength = 0;
    length = 0;
    Open(filename);
    SetFilename((char*)filename);
}

tLinuxFileMap::~tLinuxFileMap()
{
    Close();
}

void tLinuxFileMap::Open(const char* filename)
{
    int fh = open(filename, O_RDONLY);
    if(fh == -1)
        return;

    struct stat st;
    if(fstat(fh, &st) != 0 || st.st_size <= 0)
    {
        close(fh);
        return;
    }
    length = (unsigned)st.st_size;

    void* memory = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fh, 0);
    close(fh);                                   // the mapping stays valid after close()
    if(memory == MAP_FAILED)
    {
        length = 0;
        return;
    }
    mapped = memory;
    mappedLength = length;

    dataStream->Release();
    dataStream = new radLoadDataStream( (unsigned char*)memory, length, del );
}

void tLinuxFileMap::Close()
{
    if(mapped)
    {
        dataStream->Release();
        munmap(mapped, mappedLength);
        mapped = NULL;
        mappedLength = 0;
    }
    length = 0;
}

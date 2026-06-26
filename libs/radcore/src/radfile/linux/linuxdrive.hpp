//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// Linux port of win32/win32drive.hpp: a POSIX file drive.
//
// Linux has no drive letters, so every drive ("C:", "D:", ...) maps to the game directory (the working
// directory at startup), which is what relative paths such as "C:art\foo.p3d" meant on Windows.
// Windows paths are case-insensitive and the game's data was authored on Windows, so every path is
// resolved case-insensitively, component by component, when an exact match does not exist.
//=============================================================================

#ifndef LINUXDRIVE_HPP
#define LINUXDRIVE_HPP

#include <windows.h>
#include "../common/drive.hpp"
#include "../common/drivethread.hpp"

#define LINUX_DEFAULT_SECTOR_SIZE  512

// Every physical drive type must provide a drive factory.
void radLinuxDriveFactory( radDrive** ppDrive, const char* driveSpec, radMemoryAllocator alloc );

// Resolve a game path (backslashes, any case) to a real path below the game directory.
// For writes, the last component may not exist yet: parents are resolved, the leaf is kept as given.
void radLinuxResolvePath( const char* gamePath, char* realPath, unsigned int size, bool leafMayNotExist );

class radLinuxDrive : public radDrive
{
public:
    radLinuxDrive( const char* pdrivespec, radMemoryAllocator alloc );
    virtual ~radLinuxDrive( void );

    void Lock( void );
    void Unlock( void );

    unsigned int GetCapabilities( void );

    const char* GetDriveName( void );

    CompletionStatus Initialize( void );

    CompletionStatus OpenFile( const char*        fileName,
                               radFileOpenFlags   flags,
                               bool               writeAccess,
                               radFileHandle*     pHandle,
                               unsigned int*      pSize );

    CompletionStatus CloseFile( radFileHandle handle, const char* fileName );

    CompletionStatus ReadFile( radFileHandle      handle,
                               const char*        fileName,
                               IRadFile::BufferedReadState buffState,
                               unsigned int       position,
                               void*              pData,
                               unsigned int       bytesToRead,
                               unsigned int*      bytesRead,
                               radMemorySpace     pDataSpace );

    CompletionStatus WriteFile( radFileHandle     handle,
                                const char*       fileName,
                                IRadFile::BufferedReadState buffState,
                                unsigned int      position,
                                const void*       pData,
                                unsigned int      bytesToWrite,
                                unsigned int*     bytesWritten,
                                unsigned int*     size,
                                radMemorySpace    pDataSpace );

    CompletionStatus FindFirst( const char*                 searchSpec,
                                IRadDrive::DirectoryInfo*   pDirectoryInfo,
                                radFileDirHandle*           pHandle,
                                bool                        firstSearch );

    CompletionStatus FindNext( radFileDirHandle* pHandle, IRadDrive::DirectoryInfo* pDirectoryInfo );

    CompletionStatus FindClose( radFileDirHandle* pHandle );

    CompletionStatus CreateDir( const char* pName );

    CompletionStatus DestroyDir( const char* pName );

    CompletionStatus DestroyFile( const char* filename );

private:
    void SetMediaInfo( void );
    radFileError TranslateError( int posixError );

    unsigned int    m_Capabilities;
    unsigned int    m_OpenFiles;
    char            m_DriveName[ radFileDrivenameMax + 1 ];

    IRadThreadMutex*    m_pMutex;
};

#endif // LINUXDRIVE_HPP

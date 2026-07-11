// Mod Launcher-format mods: CustomFiles overlay and CustomLimits. See shar_mods.h.
#include "shar_mods.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

struct ModFile
{
    char* key;              // normalised game path: lower case, '/' separators ("art/l1z1.p3d")
    char* path;             // real path on disk: the file, or the .lmlm package holding it
    uint64_t offset;        // where the data starts in path (0 for a loose file)
    unsigned int size;
    bool packaged;
    int order;              // mod index * large + discovery order, so earlier mods sort first among equal keys
};

struct Limit
{
    char* section;
    char* key;
    int value;
};

ModFile* s_Files = NULL;
int s_NumFiles = 0, s_CapFiles = 0;
Limit* s_Limits = NULL;
int s_NumLimits = 0, s_CapLimits = 0;
bool s_Log = false;
pthread_once_t s_Once = PTHREAD_ONCE_INIT;

const char* const SUPPORTED_HACKS[] = { "CustomFiles", "CustomLimits" };

char* Dup( const char* s )
{
    size_t n = strlen( s ) + 1;
    char* d = (char*)malloc( n );
    memcpy( d, s, n );
    return d;
}

// Game path -> key: drop a drive letter, '\' -> '/', lower case, resolve "." / "..", no leading or doubled '/'.
void Normalise( const char* in, char* out, size_t size )
{
    if ( in[ 0 ] != '\0' && in[ 1 ] == ':' )
    {
        in += 2;
    }
    size_t len = 0;
    out[ 0 ] = '\0';
    while ( *in != '\0' )
    {
        while ( *in == '/' || *in == '\\' )
        {
            in++;
        }
        const char* start = in;
        while ( *in != '\0' && *in != '/' && *in != '\\' )
        {
            in++;
        }
        size_t n = (size_t)( in - start );
        if ( n == 0 || ( n == 1 && start[ 0 ] == '.' ) )
        {
            continue;
        }
        if ( n == 2 && start[ 0 ] == '.' && start[ 1 ] == '.' )
        {
            while ( len > 0 && out[ len - 1 ] != '/' )
            {
                len--;
            }
            if ( len > 0 )
            {
                len--;
            }
            out[ len ] = '\0';
            continue;
        }
        if ( len + ( len ? 1 : 0 ) + n + 1 > size )
        {
            break;
        }
        if ( len )
        {
            out[ len++ ] = '/';
        }
        for ( size_t i = 0; i < n; i++ )
        {
            out[ len++ ] = (char)tolower( (unsigned char)start[ i ] );
        }
        out[ len ] = '\0';
    }
}

// An entry of dir matching name case-insensitively, as a full path (malloc'd), or NULL.
char* FindNoCase( const char* dir, const char* name )
{
    DIR* d = opendir( dir );
    if ( d == NULL )
    {
        return NULL;
    }
    char* found = NULL;
    struct dirent* e;
    while ( ( e = readdir( d ) ) != NULL )
    {
        if ( strcasecmp( e->d_name, name ) == 0 )
        {
            found = (char*)malloc( strlen( dir ) + strlen( e->d_name ) + 2 );
            sprintf( found, "%s/%s", dir, e->d_name );
            break;
        }
    }
    closedir( d );
    return found;
}

void AddFile( const char* rel, const char* path, uint64_t offset, unsigned int size, bool packaged, int modIndex )
{
    if ( s_NumFiles == s_CapFiles )
    {
        s_CapFiles = s_CapFiles ? s_CapFiles * 2 : 256;
        s_Files = (ModFile*)realloc( s_Files, s_CapFiles * sizeof( ModFile ) );
    }
    char key[ PATH_MAX ];
    Normalise( rel, key, sizeof( key ) );
    ModFile& f = s_Files[ s_NumFiles ];
    f.key = Dup( key );
    f.path = Dup( path );
    f.offset = offset;
    f.size = size;
    f.packaged = packaged;
    f.order = modIndex * 1000000 + s_NumFiles;
    s_NumFiles++;
}

// Index every file under dir; rel is its game path so far.
int ScanTree( const char* dir, const char* rel, int modIndex )
{
    DIR* d = opendir( dir );
    if ( d == NULL )
    {
        return 0;
    }
    int count = 0;
    struct dirent* e;
    while ( ( e = readdir( d ) ) != NULL )
    {
        if ( strcmp( e->d_name, "." ) == 0 || strcmp( e->d_name, ".." ) == 0 )
        {
            continue;
        }
        char path[ PATH_MAX ], childRel[ PATH_MAX ];
        snprintf( path, sizeof( path ), "%s/%s", dir, e->d_name );
        snprintf( childRel, sizeof( childRel ), "%s%s%s", rel, rel[ 0 ] ? "/" : "", e->d_name );
        struct stat st;
        if ( stat( path, &st ) != 0 )
        {
            continue;
        }
        if ( S_ISDIR( st.st_mode ) )
        {
            count += ScanTree( path, childRel, modIndex );
        }
        else if ( S_ISREG( st.st_mode ) )
        {
            AddFile( childRel, path, 0, (unsigned int)st.st_size, false, modIndex );
            count++;
        }
    }
    closedir( d );
    return count;
}

//=============================================================================
// .lmlm packages ("LSPA"): 512-byte sectors, one field per sector (format notes in mod/tools/lmlm_extract.py)
//=============================================================================

struct PackageEntry
{
    char* rel;              // path inside the package, '/' separators, original case
    uint64_t offset;
    uint64_t size;
};

struct Package
{
    int fd;
    uint64_t fileSize;
    unsigned char sector[ 512 ];
    unsigned int pos;       // next sector to read
    PackageEntry* entries;
    int numEntries, capEntries;
    const char* error;
};

const unsigned int SECTOR = 512;
const unsigned int FLAG_TIMESTAMP = 0x02;

uint16_t U16( const unsigned char* p ) { return (uint16_t)( p[ 0 ] | p[ 1 ] << 8 ); }
uint64_t U64( const unsigned char* p )
{
    uint64_t v = 0;
    for ( int i = 7; i >= 0; i-- )
    {
        v = v << 8 | p[ i ];
    }
    return v;
}

bool NextSector( Package& pkg )
{
    if ( (uint64_t)( pkg.pos + 1 ) * SECTOR > pkg.fileSize ||
         pread( pkg.fd, pkg.sector, SECTOR, (off_t)pkg.pos * SECTOR ) != (ssize_t)SECTOR )
    {
        pkg.error = "directory runs past the end of the file";
        return false;
    }
    pkg.pos++;
    return true;
}

bool ParseDir( Package& pkg, const char* rel, unsigned int count, unsigned int firstType, int depth )
{
    if ( depth > 64 )
    {
        pkg.error = "directories nested too deeply";
        return false;
    }
    for ( unsigned int i = 0; i < count; i++ )
    {
        unsigned int type = firstType;
        if ( i > 0 )
        {
            if ( !NextSector( pkg ) )
            {
                return false;
            }
            type = pkg.sector[ 0 ];
        }
        if ( !NextSector( pkg ) )
        {
            return false;
        }
        unsigned int flags = U16( pkg.sector );
        if ( flags & ~FLAG_TIMESTAMP )
        {
            pkg.error = "unknown entry flags";
            return false;
        }
        char name[ 256 ];                               // UTF-16LE name; mod paths are ASCII in practice
        unsigned int n = 0;
        for ( unsigned int at = 2; at + 1 < SECTOR && n + 1 < sizeof( name ); at += 2 )
        {
            uint16_t c = U16( pkg.sector + at );
            if ( c == 0 )
            {
                break;
            }
            name[ n++ ] = c < 0x80 ? (char)c : '_';
        }
        name[ n ] = '\0';
        if ( n == 0 || strcmp( name, "." ) == 0 || strcmp( name, ".." ) == 0 || strchr( name, '/' ) || strchr( name, '\\' ) )
        {
            pkg.error = "bad entry name";
            return false;
        }
        char childRel[ PATH_MAX ];
        snprintf( childRel, sizeof( childRel ), "%s%s%s", rel, rel[ 0 ] ? "/" : "", name );
        if ( !NextSector( pkg ) )
        {
            return false;
        }
        const unsigned char* info = pkg.sector + 2 + ( ( flags & FLAG_TIMESTAMP ) ? 10 : 0 );
        if ( type == 1 )
        {
            if ( !ParseDir( pkg, childRel, U16( info ), info[ 2 ], depth + 1 ) )
            {
                return false;
            }
        }
        else if ( type == 0 )
        {
            uint64_t size = U64( info ), offset = U64( info + 8 );
            if ( offset > pkg.fileSize || size > pkg.fileSize - offset || size > 0xFFFFFFFFu )
            {
                pkg.error = "file data past the end of the package";
                return false;
            }
            if ( pkg.numEntries == pkg.capEntries )
            {
                pkg.capEntries = pkg.capEntries ? pkg.capEntries * 2 : 256;
                pkg.entries = (PackageEntry*)realloc( pkg.entries, pkg.capEntries * sizeof( PackageEntry ) );
            }
            PackageEntry& e = pkg.entries[ pkg.numEntries++ ];
            e.rel = Dup( childRel );
            e.offset = offset;
            e.size = size;
        }
        else
        {
            pkg.error = "unknown entry type";
            return false;
        }
    }
    return true;
}

bool OpenPackage( const char* path, Package& pkg )
{
    memset( &pkg, 0, sizeof( pkg ) );
    pkg.fd = open( path, O_RDONLY );
    struct stat st;
    if ( pkg.fd < 0 || fstat( pkg.fd, &st ) != 0 )
    {
        pkg.error = "cannot open";
        return false;
    }
    pkg.fileSize = (uint64_t)st.st_size;
    if ( !NextSector( pkg ) || memcmp( pkg.sector, "LSPA", 4 ) != 0 )
    {
        pkg.error = "not an LSPA package";
        return false;
    }
    pkg.pos = 2;                                        // root directory record
    if ( !NextSector( pkg ) )
    {
        return false;
    }
    return ParseDir( pkg, "", U16( pkg.sector + 2 ), pkg.sector[ 4 ], 0 );
}

void ClosePackage( Package& pkg )
{
    for ( int i = 0; i < pkg.numEntries; i++ )
    {
        free( pkg.entries[ i ].rel );
    }
    free( pkg.entries );
    if ( pkg.fd >= 0 )
    {
        close( pkg.fd );
    }
}

//=============================================================================
// INI files
//=============================================================================

// Minimal INI reader: [section], key=value, ';' / '#' comments, UTF-8 BOM, CRLF.
typedef void ( *IniCallback )( const char* section, const char* key, const char* value, void* user );

char* Trim( char* s )
{
    while ( isspace( (unsigned char)*s ) )
    {
        s++;
    }
    char* end = s + strlen( s );
    while ( end > s && isspace( (unsigned char)end[ -1 ] ) )
    {
        *--end = '\0';
    }
    return s;
}

// Parses text (modified in place).
void ParseIni( char* text, IniCallback cb, void* user )
{
    if ( (unsigned char)text[ 0 ] == 0xEF && (unsigned char)text[ 1 ] == 0xBB && (unsigned char)text[ 2 ] == 0xBF )
    {
        text += 3;
    }
    char section[ 128 ] = "";
    for ( char* save = NULL, *line = strtok_r( text, "\r\n", &save ); line != NULL; line = strtok_r( NULL, "\r\n", &save ) )
    {
        char* s = Trim( line );
        if ( s[ 0 ] == '\0' || s[ 0 ] == ';' || s[ 0 ] == '#' )
        {
            continue;
        }
        if ( s[ 0 ] == '[' )
        {
            char* end = strchr( s, ']' );
            if ( end != NULL )
            {
                *end = '\0';
                snprintf( section, sizeof( section ), "%s", Trim( s + 1 ) );
            }
            continue;
        }
        char* eq = strchr( s, '=' );
        if ( eq == NULL )
        {
            continue;
        }
        *eq = '\0';
        cb( section, Trim( s ), Trim( eq + 1 ), user );
    }
}

// A mod's root-level file, NUL-terminated (malloc'd), or NULL. pkg is NULL for a mod folder.
char* ReadModFile( const char* root, Package* pkg, const char* name )
{
    if ( pkg != NULL )
    {
        for ( int i = 0; i < pkg->numEntries; i++ )
        {
            const PackageEntry& e = pkg->entries[ i ];
            if ( strcasecmp( e.rel, name ) == 0 )
            {
                char* data = (char*)malloc( e.size + 1 );
                if ( pread( pkg->fd, data, e.size, (off_t)e.offset ) != (ssize_t)e.size )
                {
                    free( data );
                    return NULL;
                }
                data[ e.size ] = '\0';
                return data;
            }
        }
        return NULL;
    }
    char* path = FindNoCase( root, name );
    if ( path == NULL )
    {
        return NULL;
    }
    FILE* f = fopen( path, "rb" );
    free( path );
    if ( f == NULL )
    {
        return NULL;
    }
    fseek( f, 0, SEEK_END );
    long size = ftell( f );
    fseek( f, 0, SEEK_SET );
    char* data = (char*)malloc( size + 1 );
    size_t got = fread( data, 1, size, f );
    data[ got ] = '\0';
    fclose( f );
    return data;
}

bool HasModFile( const char* root, Package* pkg, const char* name )
{
    if ( pkg != NULL )
    {
        for ( int i = 0; i < pkg->numEntries; i++ )
        {
            if ( strcasecmp( pkg->entries[ i ].rel, name ) == 0 )
            {
                return true;
            }
        }
        return false;
    }
    char* path = FindNoCase( root, name );
    free( path );
    return path != NULL;
}

bool ReadModIni( const char* root, Package* pkg, const char* name, IniCallback cb, void* user )
{
    char* text = ReadModFile( root, pkg, name );
    if ( text == NULL )
    {
        return false;
    }
    ParseIni( text, cb, user );
    free( text );
    return true;
}

//=============================================================================
// Loading
//=============================================================================

struct MetaInfo
{
    char title[ 256 ];
    char unsupported[ 2048 ];
    char requiredMods[ 1024 ];
};

void Append( char* list, size_t size, const char* item )
{
    size_t len = strlen( list );
    snprintf( list + len, size - len, "%s%s", len ? ", " : "", item );
}

void OnMeta( const char* section, const char* key, const char* value, void* user )
{
    MetaInfo* meta = (MetaInfo*)user;
    if ( strcasecmp( section, "Miscellaneous" ) != 0 )
    {
        return;
    }
    if ( strcasecmp( key, "Title" ) == 0 )
    {
        snprintf( meta->title, sizeof( meta->title ), "%s", value );
    }
    else if ( strcasecmp( key, "RequiredHack" ) == 0 )
    {
        bool supported = false;
        for ( unsigned i = 0; i < sizeof( SUPPORTED_HACKS ) / sizeof( SUPPORTED_HACKS[ 0 ] ); i++ )
        {
            supported |= strcasecmp( value, SUPPORTED_HACKS[ i ] ) == 0;
        }
        if ( !supported )
        {
            Append( meta->unsupported, sizeof( meta->unsupported ), value );
        }
    }
    else if ( strcasecmp( key, "RequiredMod" ) == 0 )
    {
        Append( meta->requiredMods, sizeof( meta->requiredMods ), value );
    }
}

void OnLimit( const char* section, const char* key, const char* value, void* )
{
    char* end;
    long v = strtol( value, &end, 10 );
    if ( end == value )
    {
        return;
    }
    for ( int i = 0; i < s_NumLimits; i++ )
    {
        if ( strcasecmp( s_Limits[ i ].section, section ) == 0 && strcasecmp( s_Limits[ i ].key, key ) == 0 )
        {
            if ( v > s_Limits[ i ].value )
            {
                s_Limits[ i ].value = (int)v;
            }
            return;
        }
    }
    if ( s_NumLimits == s_CapLimits )
    {
        s_CapLimits = s_CapLimits ? s_CapLimits * 2 : 16;
        s_Limits = (Limit*)realloc( s_Limits, s_CapLimits * sizeof( Limit ) );
    }
    Limit& l = s_Limits[ s_NumLimits++ ];
    l.section = Dup( section );
    l.key = Dup( key );
    l.value = (int)v;
}

void LoadMod( const char* root, int modIndex )
{
    struct stat st;
    if ( stat( root, &st ) != 0 )
    {
        fprintf( stderr, "mods: %s not found\n", root );
        return;
    }
    Package pkg;
    Package* pPkg = NULL;
    int files = 0;
    if ( S_ISDIR( st.st_mode ) )
    {
        char* customFiles = FindNoCase( root, "CustomFiles" );
        if ( customFiles != NULL )
        {
            files = ScanTree( customFiles, "", modIndex );
            free( customFiles );
        }
    }
    else
    {
        if ( !OpenPackage( root, pkg ) )
        {
            fprintf( stderr, "mods: %s: %s (expected a mod folder or a .lmlm package)\n", root, pkg.error );
            ClosePackage( pkg );
            return;
        }
        pPkg = &pkg;
        static const char PREFIX[] = "CustomFiles/";
        for ( int i = 0; i < pkg.numEntries; i++ )
        {
            const PackageEntry& e = pkg.entries[ i ];
            if ( strncasecmp( e.rel, PREFIX, sizeof( PREFIX ) - 1 ) == 0 )
            {
                AddFile( e.rel + sizeof( PREFIX ) - 1, root, e.offset, (unsigned int)e.size, true, modIndex );
                files++;
            }
        }
    }

    MetaInfo meta;
    meta.title[ 0 ] = meta.unsupported[ 0 ] = meta.requiredMods[ 0 ] = '\0';
    if ( !ReadModIni( root, pPkg, "Meta.ini", OnMeta, &meta ) )
    {
        fprintf( stderr, "mods: %s has no Meta.ini; loading its CustomFiles anyway\n", root );
    }
    ReadModIni( root, pPkg, "CustomLimits.ini", OnLimit, NULL );
    fprintf( stderr, "mods: loaded \"%s\" (%s): %d files\n", meta.title[ 0 ] ? meta.title : root, root, files );
    if ( meta.unsupported[ 0 ] )
    {
        fprintf( stderr, "mods:   not supported yet, ignored: %s\n", meta.unsupported );
    }
    if ( meta.requiredMods[ 0 ] )
    {
        fprintf( stderr, "mods:   also needs these mods: %s\n", meta.requiredMods );
    }
    if ( HasModFile( root, pPkg, "CustomFiles.lua" ) || HasModFile( root, pPkg, "CustomFiles.ini" ) )
    {
        fprintf( stderr, "mods:   uses Lua (CustomFiles.lua / CustomFiles.ini path handlers): not supported yet, ignored\n" );
    }
    if ( pPkg != NULL )
    {
        ClosePackage( pkg );
    }
}

int CompareFiles( const void* a, const void* b )
{
    const ModFile* fa = (const ModFile*)a;
    const ModFile* fb = (const ModFile*)b;
    int c = strcmp( fa->key, fb->key );
    return c != 0 ? c : ( fa->order < fb->order ? -1 : fa->order > fb->order );
}

void Init( void )
{
    const char* log = getenv( "SHAR_MOD_LOG" );
    s_Log = log != NULL && log[ 0 ] != '\0' && log[ 0 ] != '0';
    const char* env = getenv( "SHAR_MODS" );
    if ( env == NULL || env[ 0 ] == '\0' )
    {
        return;
    }
    char* list = Dup( env );
    int modIndex = 0;
    for ( char* save = NULL, *tok = strtok_r( list, ":", &save ); tok != NULL; tok = strtok_r( NULL, ":", &save ) )
    {
        LoadMod( tok, modIndex++ );
    }
    free( list );

    // Sort by key, keeping only the first mod's copy of each file
    qsort( s_Files, s_NumFiles, sizeof( ModFile ), CompareFiles );
    int out = 0;
    for ( int i = 0; i < s_NumFiles; i++ )
    {
        if ( out > 0 && strcmp( s_Files[ out - 1 ].key, s_Files[ i ].key ) == 0 )
        {
            free( s_Files[ i ].key );
            free( s_Files[ i ].path );
            continue;
        }
        s_Files[ out++ ] = s_Files[ i ];
    }
    s_NumFiles = out;
    for ( int i = 0; i < s_NumLimits; i++ )
    {
        fprintf( stderr, "mods: limit [%s] %s=%d\n", s_Limits[ i ].section, s_Limits[ i ].key, s_Limits[ i ].value );
    }
}

int CompareKey( const void* key, const void* elem )
{
    return strcmp( (const char*)key, ( (const ModFile*)elem )->key );
}

const ModFile* Find( const char* gamePath )
{
    pthread_once( &s_Once, Init );
    if ( s_NumFiles == 0 )
    {
        return NULL;
    }
    char key[ PATH_MAX ];
    Normalise( gamePath, key, sizeof( key ) );
    return (const ModFile*)bsearch( key, s_Files, s_NumFiles, sizeof( ModFile ), CompareKey );
}

} // namespace

extern "C" int shar_ModHasFile( const char* gamePath )
{
    return Find( gamePath ) != NULL;
}

extern "C" int shar_ModFindFile( const char* gamePath, SharModFile* file )
{
    const ModFile* f = Find( gamePath );
    if ( f == NULL )
    {
        return 0;
    }
    file->path = f->path;
    file->offset = f->offset;
    file->size = f->size;
    file->packaged = f->packaged;
    if ( s_Log )
    {
        if ( f->packaged )
        {
            fprintf( stderr, "mods: %s -> %s @%llu (%u bytes)\n", gamePath, f->path, (unsigned long long)f->offset, f->size );
        }
        else
        {
            fprintf( stderr, "mods: %s -> %s\n", gamePath, f->path );
        }
    }
    return 1;
}

extern "C" int shar_ModLimit( const char* section, const char* key, int defaultValue )
{
    pthread_once( &s_Once, Init );
    for ( int i = 0; i < s_NumLimits; i++ )
    {
        if ( strcasecmp( s_Limits[ i ].section, section ) == 0 && strcasecmp( s_Limits[ i ].key, key ) == 0 )
        {
            return s_Limits[ i ].value;
        }
    }
    return defaultValue;
}

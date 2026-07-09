// Mod Launcher-format mods: CustomFiles overlay and CustomLimits. See shar_mods.h.
#include "shar_mods.h"
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

namespace {

struct ModFile
{
    char* key;      // normalised game path: lower case, '/' separators ("art/l1z1.p3d")
    char* path;     // real path on disk
    int order;      // mod index * large + discovery order, so earlier mods sort first among equal keys
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

void AddFile( const char* key, const char* path, int order )
{
    if ( s_NumFiles == s_CapFiles )
    {
        s_CapFiles = s_CapFiles ? s_CapFiles * 2 : 256;
        s_Files = (ModFile*)realloc( s_Files, s_CapFiles * sizeof( ModFile ) );
    }
    ModFile& f = s_Files[ s_NumFiles++ ];
    f.key = Dup( key );
    f.path = Dup( path );
    f.order = order;
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
            char key[ PATH_MAX ];
            Normalise( childRel, key, sizeof( key ) );
            AddFile( key, path, modIndex * 1000000 + s_NumFiles );
            count++;
        }
    }
    closedir( d );
    return count;
}

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

bool ReadIni( const char* path, IniCallback cb, void* user )
{
    FILE* f = fopen( path, "rb" );
    if ( f == NULL )
    {
        return false;
    }
    char section[ 128 ] = "";
    char line[ 4096 ];
    bool first = true;
    while ( fgets( line, sizeof( line ), f ) != NULL )
    {
        char* s = line;
        if ( first && (unsigned char)s[ 0 ] == 0xEF && (unsigned char)s[ 1 ] == 0xBB && (unsigned char)s[ 2 ] == 0xBF )
        {
            s += 3;
        }
        first = false;
        s = Trim( s );
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
    fclose( f );
    return true;
}

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
    if ( stat( root, &st ) != 0 || !S_ISDIR( st.st_mode ) )
    {
        fprintf( stderr, "mods: %s is not a folder (extract .lmlm packages with mod/tools/lmlm_extract.py)\n", root );
        return;
    }
    MetaInfo meta;
    meta.title[ 0 ] = meta.unsupported[ 0 ] = meta.requiredMods[ 0 ] = '\0';
    char* metaPath = FindNoCase( root, "Meta.ini" );
    if ( metaPath == NULL || !ReadIni( metaPath, OnMeta, &meta ) )
    {
        fprintf( stderr, "mods: %s has no Meta.ini; loading its CustomFiles anyway\n", root );
    }
    free( metaPath );

    int files = 0;
    char* customFiles = FindNoCase( root, "CustomFiles" );
    if ( customFiles != NULL )
    {
        files = ScanTree( customFiles, "", modIndex );
        free( customFiles );
    }
    char* limits = FindNoCase( root, "CustomLimits.ini" );
    if ( limits != NULL )
    {
        ReadIni( limits, OnLimit, NULL );
        free( limits );
    }
    fprintf( stderr, "mods: loaded \"%s\" (%s): %d files\n", meta.title[ 0 ] ? meta.title : root, root, files );
    if ( meta.unsupported[ 0 ] )
    {
        fprintf( stderr, "mods:   not supported yet, ignored: %s\n", meta.unsupported );
    }
    if ( meta.requiredMods[ 0 ] )
    {
        fprintf( stderr, "mods:   also needs these mods: %s\n", meta.requiredMods );
    }
    char* lua = FindNoCase( root, "CustomFiles.lua" );
    char* handlers = FindNoCase( root, "CustomFiles.ini" );
    if ( lua != NULL || handlers != NULL )
    {
        fprintf( stderr, "mods:   uses Lua (CustomFiles.lua / CustomFiles.ini path handlers): not supported yet, ignored\n" );
    }
    free( lua );
    free( handlers );
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

extern "C" const char* shar_ModFindFile( const char* gamePath )
{
    const ModFile* f = Find( gamePath );
    if ( f == NULL )
    {
        return NULL;
    }
    if ( s_Log )
    {
        fprintf( stderr, "mods: %s -> %s\n", gamePath, f->path );
    }
    return f->path;
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

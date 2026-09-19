/* Mod Launcher CustomCarSupport.ini (see shar_modcars.h). */
#include "shar_modcars.h"
#include "shar_mods.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace
{

const int MAX_CARS = 128;

SharModCar s_cars[ MAX_CARS ];
int s_numCars = 0;
bool s_loaded = false;

char* Trim( char* s )
{
    while( *s == ' ' || *s == '\t' )
    {
        s++;
    }
    char* e = s + strlen( s );
    while( e > s && ( e[ -1 ] == ' ' || e[ -1 ] == '\t' || e[ -1 ] == '\r' ) )
    {
        *--e = '\0';
    }
    return s;
}

SharModCar* NewCar( const char* name )
{
    if( s_numCars >= MAX_CARS )
    {
        return NULL;
    }
    SharModCar* car = &s_cars[ s_numCars++ ];
    snprintf( car->name, sizeof( car->name ), "%s", name );
    car->index = -1;
    car->noHusk = car->noSkidMarks = car->noFrontSkidMarks = car->shadowVisible = car->abductable =
        car->recolourable = car->invincible = car->noBumperCam = -1;
    return car;
}

void Load()
{
    s_loaded = true;
    char* text = shar_ModConfig( "CustomCarSupport.ini" );
    if( text == NULL )
    {
        return;
    }

    // [Car] sections get their name from Name=, which may follow other keys: they are collected first
    char* start = text;
    if( (unsigned char) start[ 0 ] == 0xEF && (unsigned char) start[ 1 ] == 0xBB && (unsigned char) start[ 2 ] == 0xBF )
    {
        start += 3; // UTF-8 byte order mark
    }
    SharModCar* car = NULL;
    bool inCar = false;         // a [Car] section, named or not yet
    for( char* line = strtok( start, "\n" ); line != NULL; line = strtok( NULL, "\n" ) )
    {
        line = Trim( line );
        if( *line == '\0' || *line == ';' || *line == '#' )
        {
            continue;
        }
        if( *line == '[' )
        {
            char* end = strchr( line, ']' );
            if( end != NULL )
            {
                *end = '\0';
            }
            char* section = Trim( line + 1 );
            car = NULL;
            inCar = false;
            if( strcasecmp( section, "Car" ) == 0 )
            {
                inCar = true;
                car = NewCar( "" );
            }
            else if( strcasecmp( section, "FakeWheel" ) != 0 )
            {
                car = NewCar( section );
            }
            continue;
        }
        char* eq = strchr( line, '=' );
        if( car == NULL || eq == NULL )
        {
            continue;
        }
        *eq = '\0';
        char* key = Trim( line );
        char* value = Trim( eq + 1 );
        int number = atoi( value );
        if( strcasecmp( key, "Name" ) == 0 && inCar )
        {
            snprintf( car->name, sizeof( car->name ), "%s", value );
        }
        else if( strcasecmp( key, "Index" ) == 0 )
        {
            car->index = number;
        }
        else if( strcasecmp( key, "NoHusk" ) == 0 )
        {
            car->noHusk = number != 0;
        }
        else if( strcasecmp( key, "NoSkidMarks" ) == 0 )
        {
            car->noSkidMarks = number != 0;
        }
        else if( strcasecmp( key, "NoFrontSkidMarks" ) == 0 )
        {
            car->noFrontSkidMarks = number != 0;
        }
        else if( strcasecmp( key, "ShadowVisible" ) == 0 )
        {
            car->shadowVisible = number != 0;
        }
        else if( strcasecmp( key, "Abductable" ) == 0 )
        {
            car->abductable = number != 0;
        }
        else if( strcasecmp( key, "Recolourable" ) == 0 )
        {
            car->recolourable = number != 0;
        }
        else if( strcasecmp( key, "Invincible" ) == 0 )
        {
            car->invincible = number != 0;
        }
        else if( strcasecmp( key, "NoBumperCam" ) == 0 )
        {
            car->noBumperCam = number != 0;
        }
    }
    free( text );

    if( getenv( "SHAR_MOD_LOG" ) != NULL )
    {
        for( int i = 0; i < s_numCars; i++ )
        {
            const SharModCar* c = &s_cars[ i ];
            fprintf( stderr, "mods: CustomCarSupport %s: index %d, nohusk %d, noskid %d/%d, shadow %d, abductable %d, "
                     "recolourable %d, invincible %d, nobumpercam %d\n", c->name, c->index, c->noHusk, c->noSkidMarks,
                     c->noFrontSkidMarks, c->shadowVisible, c->abductable, c->recolourable, c->invincible,
                     c->noBumperCam );
        }
    }
}

}

const SharModCar* shar_ModCar( const char* name )
{
    if( !s_loaded )
    {
        Load();
    }
    if( name == NULL )
    {
        return NULL;
    }
    for( int i = 0; i < s_numCars; i++ )
    {
        if( s_cars[ i ].name[ 0 ] != '\0' && strcasecmp( s_cars[ i ].name, name ) == 0 )
        {
            return &s_cars[ i ];
        }
    }
    return NULL;
}

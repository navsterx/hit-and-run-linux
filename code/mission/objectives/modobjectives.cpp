//=============================================================================
// Linux port: Mod Launcher AdditionalScriptFunctionality objective types (see modobjectives.h)
//=============================================================================
#ifdef RAD_LINUX

#include <raddebug.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <mission/objectives/modobjectives.h>
#include <mission/gameplaymanager.h>
#include <mission/charactersheet/charactersheetmanager.h>
#include <events/eventmanager.h>
#include <meta/locatorevents.h>
#include <camera/animatedcam.h>
#include <camera/supercammanager.h>
#include <presentation/gui/guisystem.h>
#include <presentation/gui/ingame/guimanageringame.h>
#include <presentation/gui/ingame/guiscreenhud.h>
#include <worldsim/avatar.h>
#include <worldsim/avatarmanager.h>
#include <worldsim/hitnrunmanager.h>
#include <worldsim/character/character.h>
#include <worldsim/character/charactermanager.h>
#include <worldsim/redbrick/vehicle.h>

namespace
{

struct KindName
{
    const char* name;
    ModCounter::Kind kind;
};

const KindName KIND_NAMES[] =
{
    { "collectcoins", ModCounter::COLLECT_COINS },
    { "collectwrench", ModCounter::COLLECT_WRENCH },
    { "completedwasps", ModCounter::COMPLETED_WASPS },
    { "destroycars", ModCounter::DESTROY_CARS },
    { "event", ModCounter::EVENT },
    { "hitandrun", ModCounter::HIT_AND_RUN },
    { "hitandruncaught", ModCounter::HIT_AND_RUN_CAUGHT },
    { "hitandrunlost", ModCounter::HIT_AND_RUN_LOST },
    { "hitandrunrage", ModCounter::HIT_AND_RUN_RAGE },
    { "hitpeds", ModCounter::HIT_PEDS },
    { "jump", ModCounter::JUMP },
    { "reachspeed", ModCounter::REACH_SPEED },
    { "spring", ModCounter::SPRING },
};

// the events each kind counts (EVENT: its own)
EventEnum CountedEvent( const ModCounter& counter )
{
    switch( counter.kind )
    {
    case ModCounter::COLLECT_COINS: return EVENT_COLLECTED_COINS;
    case ModCounter::COLLECT_WRENCH: return EVENT_COLLECTED_WRENCH;
    case ModCounter::COMPLETED_WASPS: return EVENT_WASP_BLOWED_UP;
    case ModCounter::DESTROY_CARS: return EVENT_VEHICLE_DESTROYED_BY_USER;
    case ModCounter::EVENT: return static_cast< EventEnum >( counter.eventId );
    case ModCounter::HIT_AND_RUN: return EVENT_HIT_AND_RUN_START;
    case ModCounter::HIT_AND_RUN_CAUGHT: return EVENT_HIT_AND_RUN_CAUGHT;
    case ModCounter::HIT_AND_RUN_LOST: return EVENT_HIT_AND_RUN_EVADED;
    case ModCounter::HIT_PEDS: return EVENT_KICK_NPC;
    case ModCounter::JUMP: return EVENT_JUMP_TAKEOFF;
    case ModCounter::SPRING: return static_cast< EventEnum >( EVENT_LOCATOR + LocatorEvent::BOUNCEPAD );
    default: return NUM_EVENTS;     // polled
    }
}

float PlayerSpeedKmh()
{
    Avatar* avatar = GetAvatarManager()->GetAvatarForPlayer( 0 );
    Vehicle* vehicle = avatar != NULL ? avatar->GetVehicle() : NULL;
    return vehicle != NULL ? vehicle->GetSpeedKmh() : 0.0f;
}

}

int ModCounter::KindOf( const char* name )
{
    for( unsigned int i = 0; i < sizeof( KIND_NAMES ) / sizeof( KIND_NAMES[ 0 ] ); i++ )
    {
        if( strcasecmp( name, KIND_NAMES[ i ].name ) == 0 )
        {
            return KIND_NAMES[ i ].kind;
        }
    }
    return -1;
}

ModCounter::ModCounter() :
    kind( COLLECT_COINS ),
    eventId( NUM_EVENTS ),
    total( 1 ),
    count( 0 ),
    speedKmh( 0.0f ),
    numModels( 0 )
{
}

void ModCounter::Start( EventListener* listener )
{
    count = 0;
    EventEnum id = CountedEvent( *this );
    if( id >= 0 && id < NUM_EVENTS )
    {
        GetEventManager()->AddListener( listener, id );
    }
}

void ModCounter::Stop( EventListener* listener )
{
    EventEnum id = CountedEvent( *this );
    if( id >= 0 && id < NUM_EVENTS )
    {
        GetEventManager()->RemoveListener( listener, id );
    }
}

void ModCounter::AddModel( const char* name )
{
    if( numModels < MAX_MODELS )
    {
        models[ numModels++ ] = tEntity::MakeUID( name );
    }
}

bool ModCounter::OnEvent( EventEnum id, void* pEventData )
{
    if( id != CountedEvent( *this ) )
    {
        return false;
    }
    if( numModels > 0 && ( kind == HIT_PEDS || kind == DESTROY_CARS ) )
    {
        tUID uid = 0;
        if( kind == HIT_PEDS && pEventData != NULL )
        {
            uid = tEntity::MakeUID( GetCharacterManager()->GetModelName( static_cast< Character* >( pEventData ) ) );
        }
        else if( kind == DESTROY_CARS && pEventData != NULL )
        {
            uid = tEntity::MakeUID( static_cast< Vehicle* >( pEventData )->GetName() );
        }
        bool wanted = false;
        for( int i = 0; i < numModels; i++ )
        {
            wanted = wanted || models[ i ] == uid;
        }
        if( !wanted )
        {
            return false;
        }
    }
    count++;
    return true;
}

int ModCounter::Current() const
{
    switch( kind )
    {
    case COMPLETED_WASPS:
        return GetCharacterSheetManager()->QueryNumWaspsDestroyed( GetGameplayManager()->GetCurrentLevelIndex() );
    case REACH_SPEED:
        return PlayerSpeedKmh() >= speedKmh ? total : 0;
    case HIT_AND_RUN_RAGE:
        return GetHitnRunManager()->GetHitnRunValue() >= GetHitnRunManager()->GetWarningThreshold() ? total : 0;
    default:
        return count;
    }
}

//=============================================================================
// ModCountObjective
//=============================================================================
ModCountObjective::ModCountObjective( ModCounter::Kind kind, int eventId ) :
    mShownCount( -1 ),
    mShowingCount( false )
{
    mCounter.kind = kind;
    mCounter.eventId = eventId;
}

void ModCountObjective::OnInitialize()
{
    mCounter.Start( this );
    mShownCount = -1;
    // a count to show: as the game's delivery objectives do, only for more than one
    mShowingCount = mCounter.total > 1 && mCounter.kind != ModCounter::REACH_SPEED &&
                    mCounter.kind != ModCounter::HIT_AND_RUN_RAGE;
    if( mShowingCount )
    {
        GetGuiSystem()->HandleMessage( GUI_MSG_SHOW_HUD_OVERLAY, HUD_COLLECTIBLES );
    }
    ShowCount();
}

void ModCountObjective::OnFinalize()
{
    mCounter.Stop( this );
    if( mShowingCount )
    {
        GetGuiSystem()->HandleMessage( GUI_MSG_HIDE_HUD_OVERLAY, HUD_COLLECTIBLES );
        mShowingCount = false;
    }
}

void ModCountObjective::ShowCount()
{
    int current = mCounter.Current();
    if( current > mCounter.total )
    {
        current = mCounter.total;
    }
    if( mShowingCount && current != mShownCount && GetCurrentHud() != NULL )
    {
        GetCurrentHud()->SetCollectibles( current, mCounter.total );
        mShownCount = current;
    }
}

void ModCountObjective::OnUpdate( unsigned int elapsedTime )
{
    if( mCounter.kind == ModCounter::HIT_AND_RUN && GetHitnRunManager()->IsHitnRunActive() )
    {
        mCounter.count = mCounter.total;    // already in one (e.g. SetStageHitAndRun(100))
    }
    ShowCount();
    if( !GetFinished() && mCounter.Reached() )
    {
        SetFinished( true );
    }
}

void ModCountObjective::HandleEvent( EventEnum id, void* pEventData )
{
    mCounter.OnEvent( id, pEventData );
}

//=============================================================================
// ModCameraObjective
//=============================================================================
ModCameraObjective::ModCameraObjective() :
    mCanSkip( false ),
    mNoLetterbox( false ),
    mStarted( false ),
    mWaitedMs( 0 )
{
    mCamera[ 0 ] = '\0';
    mMulticontroller[ 0 ] = '\0';
}

void ModCameraObjective::SetCameraName( const char* name )
{
    snprintf( mCamera, sizeof( mCamera ), "%s", name );
}

void ModCameraObjective::SetMulticontrollerName( const char* name )
{
    snprintf( mMulticontroller, sizeof( mMulticontroller ), "%s", name );
}

void ModCameraObjective::OnInitialize()
{
    mStarted = false;
    mWaitedMs = 0;
    GetEventManager()->AddListener( this, EVENT_ANIMATED_CAM_SHUTDOWN );
    AnimatedCam::AllowSkipping( mCanSkip );
    if( mNoLetterbox )
    {
        AnimatedCam::SupressNextLetterbox();
    }
    AnimatedCam::SetMulticontroller( mMulticontroller );
    AnimatedCam::SetCamera( mCamera );
    AnimatedCam::CheckPendingCameraSwitch();    // (the game only checks when gameplay starts)
}

void ModCameraObjective::OnFinalize()
{
    GetEventManager()->RemoveListener( this, EVENT_ANIMATED_CAM_SHUTDOWN );
}

void ModCameraObjective::OnUpdate( unsigned int elapsedTime )
{
    if( GetFinished() || mStarted )
    {
        return;
    }
    SuperCam* cam = GetSuperCamManager()->GetSCC( 0 )->GetActiveSuperCam();
    if( cam != NULL && cam->GetType() == SuperCam::ANIMATED_CAM )
    {
        mStarted = true;
        if( getenv( "SHAR_MOD_LOG" ) != NULL )
        {
            fprintf( stderr, "mods: camera objective: %s / %s playing\n", mMulticontroller, mCamera );
        }
        return;
    }
    // the camera never started (missing from the inventory, or the player was inside an interior): move on
    mWaitedMs += elapsedTime;
    if( mWaitedMs > 3000 )
    {
        fprintf( stderr, "mods: camera objective: %s / %s did not start\n", mMulticontroller, mCamera );
        SetFinished( true );
    }
}

void ModCameraObjective::HandleEvent( EventEnum id, void* pEventData )
{
    if( id == EVENT_ANIMATED_CAM_SHUTDOWN )
    {
        if( getenv( "SHAR_MOD_LOG" ) != NULL )
        {
            fprintf( stderr, "mods: camera objective: %s / %s done%s\n", mMulticontroller, mCamera,
                     mStarted ? "" : " (before it played)" );
        }
        SetFinished( true );
    }
}

//=============================================================================
MissionObjective* ModCreateObjective( int argc, char** argv )
{
    if( strcasecmp( argv[ 1 ], "camera" ) == 0 )
    {
        return new ModCameraObjective();
    }
    int kind = ModCounter::KindOf( argv[ 1 ] );
    if( kind < 0 )
    {
        return NULL;
    }
    int eventId = NUM_EVENTS;
    if( kind == ModCounter::EVENT && argc > 2 )
    {
        eventId = ModTranslateEvent( atoi( argv[ 2 ] ) );
    }
    return new ModCountObjective( static_cast< ModCounter::Kind >( kind ), eventId );
}

int ModTranslateEvent( int retailEvent )
{
    const int FE_CANCEL = static_cast< int >( EVENT_FE_CANCEL );   // retail 176 = EVENT_FE_CANCEL_1, 177 = _2
    return retailEvent <= FE_CANCEL ? retailEvent : retailEvent == FE_CANCEL + 1 ? FE_CANCEL : retailEvent - 1;
}

#endif // RAD_LINUX

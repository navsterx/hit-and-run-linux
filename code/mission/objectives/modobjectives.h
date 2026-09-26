//=============================================================================
// Linux port: the objective types Lucas' Mod Launcher's AdditionalScriptFunctionality hack adds for mods' mission
// scripts (docs.donutteam.com, Hacks/AdditionalScriptFunctionality/MissionObjectives). Their script commands are in
// code/mission/modscript.cpp; the matching conditions in conditions/modconditions.h share ModCounter.
//=============================================================================
#ifndef MODOBJECTIVES_H
#define MODOBJECTIVES_H

#ifdef RAD_LINUX

#include <mission/objectives/missionobjective.h>
#include <p3d/entity.hpp>

class Character;
class Vehicle;

// Counts what an ASF objective or condition is about: events (kicked peds, destroyed cars, jumps, ...) or a state
// polled each frame (wasps destroyed, speed, Hit & Run meter)
struct ModCounter
{
    enum Kind
    {
        COLLECT_COINS, COLLECT_WRENCH, COMPLETED_WASPS, DESTROY_CARS, EVENT, HIT_AND_RUN, HIT_AND_RUN_CAUGHT,
        HIT_AND_RUN_LOST, HIT_AND_RUN_RAGE, HIT_PEDS, JUMP, REACH_SPEED, SPRING, NUM_KINDS
    };
    enum { MAX_MODELS = 16 };

    // The kind for an ASF type name ("hitpeds", ...), -1 for others
    static int KindOf( const char* name );

    ModCounter();
    void Start( EventListener* listener );      // count from 0, listen to the kind's events
    void Stop( EventListener* listener );
    void AddModel( const char* name );          // AddObjTargetModel / AddCondTargetModel
    // Counts an event the listener received; true when it was one of this counter's
    bool OnEvent( EventEnum id, void* pEventData );
    int Current() const;                        // the count, or the polled value for the polled kinds
    bool Reached() const { return Current() >= total; }

    Kind kind;
    int eventId;            // EVENT: the game's event (translated from the Mod Launcher's retail numbering)
    int total;
    int count;
    float speedKmh;         // REACH_SPEED
    int numModels;
    tUID models[ MAX_MODELS ];
};

class ModCountObjective : public MissionObjective
{
public:
    ModCountObjective( ModCounter::Kind kind, int eventId );
    ModCounter& GetCounter() { return mCounter; }
    virtual void HandleEvent( EventEnum id, void* pEventData );
protected:
    virtual void OnInitialize();
    virtual void OnFinalize();
    virtual void OnUpdate( unsigned int elapsedTime );
private:
    void ShowCount();
    ModCounter mCounter;
    int mShownCount;
    bool mShowingCount;
};

// "camera": plays an animated camera (multicontroller + camera), done when it ends
class ModCameraObjective : public MissionObjective
{
public:
    ModCameraObjective();
    void SetCameraName( const char* name );
    void SetMulticontrollerName( const char* name );
    void SetCanSkip( bool canSkip ) { mCanSkip = canSkip; }
    void SetNoLetterbox( bool noLetterbox ) { mNoLetterbox = noLetterbox; }
    virtual void HandleEvent( EventEnum id, void* pEventData );
protected:
    virtual void OnInitialize();
    virtual void OnFinalize();
    virtual void OnUpdate( unsigned int elapsedTime );
private:
    char mCamera[ 64 ];
    char mMulticontroller[ 64 ];
    bool mCanSkip;
    bool mNoLetterbox;
    bool mStarted;          // the animated camera became active
    unsigned int mWaitedMs;
};

// The objective for an ASF type name ("camera", "hitpeds", ...), NULL for other names. argv as AddObjective's.
MissionObjective* ModCreateObjective( int argc, char** argv );

// The game's event for a Mod Launcher event number (retail numbering: from 177 one higher than this source's, where
// EVENT_FE_CANCEL became _1 and _2)
int ModTranslateEvent( int retailEvent );

#endif // RAD_LINUX

#endif // MODOBJECTIVES_H

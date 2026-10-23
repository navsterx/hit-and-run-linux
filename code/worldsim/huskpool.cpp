//=============================================================================
// Copyright (C) 2003 Radical Entertainment Ltd.  All rights reserved.
//
// File:        huskpool.cpp
//
// Description: 
//
// History:     Mar 27, 2003 - created, gmayer
//
//=============================================================================


#include <worldsim/vehiclecentral.h>
#include <worldsim/redbrick/geometryvehicle.h>
#include <worldsim/redbrick/vehicle.h>
#include <worldsim/huskpool.h>
#include <worldsim/redbrick/trafficlocomotion.h>

#include <memory/srrmemory.h>

#include <mission/gameplaymanager.h>
#ifdef RAD_LINUX
#include <shar_modcars.h>
#include <p3d/anim/compositedrawable.hpp>
#include <strings.h>
#endif


//=============================================================================
// HuskPool::HuskPool
//=============================================================================
// Description: Comment
//
// Parameters:  (int num)
//
// Return:      HuskPool
//
//=============================================================================
HuskPool::HuskPool()
{
    mTotalNum = 0;
}


//=============================================================================
// ::~HuskPool
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      HuskPool
//
//=============================================================================
HuskPool::~HuskPool()
{

}


//=============================================================================
// HuskPool::Init
//=============================================================================
// Description: Comment
//
//      this has to be called from GameplayContext and DemoContext OnStart

//
// Parameters:  (int num)
//
// Return:      void 
//
//=============================================================================
void HuskPool::Init(int num)
{

MEMTRACK_PUSH_GROUP( "HuskPool Init" );

    GameMemoryAllocator gma = GetGameplayManager()->GetCurrentMissionHeap();    
    HeapMgr()->PushHeap (gma);

    mTotalNum = num;

#ifdef RAD_LINUX
    // Mod Launcher CustomCarSupport Husk=<car>: a few husks of each such car whose data is loaded by now (the docs:
    // "the husk must already be loaded in memory, or no husk is used"), after the huskA ones
    const int CUSTOM_HUSKS_PER_TYPE = 2;
    const int MAX_CUSTOM_HUSK_TYPES = 8;
    const char* customHusks[ MAX_CUSTOM_HUSK_TYPES ];
    int numCustomHusks = 0;
    for( int c = 0; c < shar_ModCarCount(); c++ )
    {
        const char* husk = shar_ModCarAt( c )->husk;
        if( husk[ 0 ] == '\0' || strcasecmp( husk, "huskA" ) == 0 || numCustomHusks >= MAX_CUSTOM_HUSK_TYPES )
        {
            continue;
        }
        bool known = false;
        for( int k = 0; k < numCustomHusks; k++ )
        {
            known = known || strcasecmp( customHusks[ k ], husk ) == 0;
        }
        if( known )
        {
            continue;
        }
        if( p3d::find<tCompositeDrawable>( husk ) == NULL )
        {
            rReleasePrintf( "mods: CustomCarSupport: husk car %s is not loaded, cars using it leave no husk\n", husk );
            continue;
        }
        customHusks[ numCustomHusks++ ] = husk;
    }
    mTotalNum = num + numCustomHusks * CUSTOM_HUSKS_PER_TYPE;
#endif

    mHuskArray = new HuskData[mTotalNum];
    
    for( int i=0; i<mTotalNum; i++)
    {
        // initially make all husks VT_TRAFFIC
        // set the husk type to whatever the vehicle that just got destroyed is...
    
#ifdef RAD_LINUX
        const char* huskName = i < num ? "huskA" : customHusks[ ( i - num ) / CUSTOM_HUSKS_PER_TYPE ];
        Vehicle* huskVehicle = GetVehicleCentral()->InitVehicle(huskName, false, 0, VT_TRAFFIC);
        huskVehicle->mModHusk = i >= num;
#else
        Vehicle* huskVehicle = GetVehicleCentral()->InitVehicle("huskA", false, 0, VT_TRAFFIC);
#endif
        rAssert( huskVehicle );

        huskVehicle->AddRef();
        huskVehicle->SetLocomotion( VL_PHYSICS );
#ifdef DEBUGWATCH
        // HACK:
        // Automatically take it out of vehicleAIRender, so we don't 
        // clutter up the render slots
        huskVehicle->mTrafficLocomotion->GetAI()->UnregisterAI();
#endif

        mHuskArray[i].huskVehicle = huskVehicle;
        mHuskArray[i].originalVehicle = NULL;
        mHuskArray[i].inUse = false;    
    }    
    HeapMgr()->PopHeap (gma);

MEMTRACK_PUSH_GROUP( "HuskPool Init" );
   
}


//=============================================================================
// HuskPool::Empty
//=============================================================================
// Description: Comment
//
//
//  called from PauseContext and DemoContext OnStop
//
// Parameters:  ()
//
// Return:      void 
//
//=============================================================================
void HuskPool::Empty()
{
    // called when you're leaving the level
    int i;
    for(i = 0; i < mTotalNum; i++)
    {
        // TODO:
        // Should do this just in case?
        //GetVehicleCentral()->RemoveVehicleFromActiveList( mHuskArray[i].huskVehicle );

        mHuskArray[i].huskVehicle->ReleaseVerified();    
        mHuskArray[i].huskVehicle = NULL;

        if( mHuskArray[i].originalVehicle )
        {
            mHuskArray[i].originalVehicle->Release();
            mHuskArray[i].originalVehicle = NULL;
        }
    }
    
    mTotalNum = 0;
    delete[] mHuskArray;
}


//=============================================================================
// HuskPool::RequestHusk
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      Vehicle
//
//=============================================================================
Vehicle* HuskPool::RequestHusk( VehicleType vt, Vehicle* originalVehicle )
{
    rAssert( originalVehicle );
    if( originalVehicle == NULL )
    {
        return NULL;
    }

    if( !WillMakeConvincingHusk( originalVehicle ) )
    {
        return NULL;
    }

#ifdef RAD_LINUX
    // the husk car this one leaves (CustomCarSupport Husk=; huskA otherwise)
    const char* wanted = "huskA";
    if( originalVehicle->mModCar != NULL && originalVehicle->mModCar->husk[ 0 ] != '\0' )
    {
        wanted = originalVehicle->mModCar->husk;
    }
#endif
    for( int i = 0; i < mTotalNum; i++ )
    {
#ifdef RAD_LINUX
        if( strcasecmp( mHuskArray[i].huskVehicle->GetName(), wanted ) != 0 )
        {
            continue;
        }
#endif
        if(mHuskArray[i].inUse == false)
        {
            // give 'em this one
            mHuskArray[i].inUse = true;
            mHuskArray[i].huskVehicle->mVehicleType = vt;   // I'm pretty sure this is safe to set on the fly
            mHuskArray[i].huskVehicle->mGeometryVehicle->SetFadeAlpha( 255 ); // restore vehicle fade alpha

            // store away the original vehicle
            rAssert( mHuskArray[i].originalVehicle == NULL );
            mHuskArray[i].originalVehicle = originalVehicle;
            mHuskArray[i].originalVehicle->AddRef();

            return mHuskArray[i].huskVehicle;            
        }
    }

    //rReleaseAssertMsg(0, "Not enough husks to go around!");
    return NULL;
}


//=============================================================================
// HuskPool::FreeHusk
//=============================================================================
// Description: Comment
//
// Parameters:  (Vehicle* husk)
//
// Return:      void 
//
//=============================================================================
void HuskPool::FreeHusk(Vehicle* husk)
{
    if( husk == NULL )
    {
        return;
    }

    for( int i = 0; i < mTotalNum; i++)
    {
        if( mHuskArray[i].huskVehicle == husk &&
			mHuskArray[i].inUse )
        {
            mHuskArray[i].inUse = false;

            rAssert( mHuskArray[i].originalVehicle );
            mHuskArray[i].originalVehicle->Release();
            mHuskArray[i].originalVehicle = NULL;

            return;
        }
    }
}

bool HuskPool::IsHuskType( Vehicle* vehicle )
{
    return vehicle != NULL && vehicle->IsHuskModel();
}

bool HuskPool::IsHuskType( VehicleEnum::VehicleID id )
{   
    // NOTE:
    // If we start using other Husk models other than "huskA", 
    // we add the ID checks here...
    if( id == VehicleEnum::HUSKA )
    {
        return true;
    }
    return false;
}


Vehicle* HuskPool::FindOriginalVehicleGivenHusk( Vehicle* husk )
{
    if( husk == NULL )
    {
        return NULL;
    }

    for( int i = 0; i < mTotalNum; i++)
    {
        if(mHuskArray[i].huskVehicle == husk)
        {
            rAssert( mHuskArray[i].inUse );
            rAssert( mHuskArray[i].originalVehicle );
            return mHuskArray[i].originalVehicle;
        }
    }

    return NULL;
}

Vehicle* HuskPool::FindHuskGivenOriginalVehicle( Vehicle* v )
{
    if( v == NULL )
    {
        return NULL;
    }

    for( int i = 0; i < mTotalNum; i++)
    {
        if(mHuskArray[i].originalVehicle == v)
        {
            rAssert( mHuskArray[i].inUse );
            rAssert( mHuskArray[i].huskVehicle );
            return mHuskArray[i].huskVehicle;
        }
    }

    return NULL;

}


bool HuskPool::WillMakeConvincingHusk( Vehicle* origV )
{
    // these cars don't look anything remotely like 
    // HuskA, so we blow them up and leave nothing behind
#ifdef RAD_LINUX
    if( origV->mModCar != NULL && origV->mModCar->noHusk >= 0 )
    {
        return origV->mModCar->noHusk == 0; // CustomCarSupport NoHusk
    }
#endif

    if( origV->mVehicleID == VehicleEnum::HONOR_V || 
        origV->mVehicleID == VehicleEnum::SCHOOLBU || 
        origV->mVehicleID == VehicleEnum::WILLI_V || 
        origV->mVehicleID == VehicleEnum::HALLO   || 
        origV->mVehicleID == VehicleEnum::OBLIT_V || 

        origV->mVehicleID == VehicleEnum::MONO_V  || 
        origV->mVehicleID == VehicleEnum::KNIGH_V || 
        origV->mVehicleID == VehicleEnum::CFIRE_V || 
        origV->mVehicleID == VehicleEnum::CCOLA   || 
        origV->mVehicleID == VehicleEnum::HBIKE_V || 

        origV->mVehicleID == VehicleEnum::ROCKE_V || 
        origV->mVehicleID == VehicleEnum::ATV_V   || 
        origV->mVehicleID == VehicleEnum::DUNE_V  || 
        origV->mVehicleID == VehicleEnum::COFFIN  || 
        origV->mVehicleID == VehicleEnum::SHIP    || 

        origV->mVehicleID == VehicleEnum::WITCHCAR )
    {
        return false;
    }
    return true;
}
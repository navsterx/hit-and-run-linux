/*===========================================================================
   suspensionjointdriver.cpp

   created Feb 1, 2002
   by Greg Mayer

   Copyright (c) 2002 Radical Entertainment, Inc.
   All rights reserved.


===========================================================================*/

#include <poser/pose.hpp>
#include <poser/poseengine.hpp>
#include <poser/posedriver.hpp>
#include <poser/joint.hpp>

#include <worldsim/redbrick/suspensionjointdriver.h>
#include <worldsim/redbrick/wheel.h>
#ifdef RAD_LINUX
#include <worldsim/redbrick/vehicle.h>
#include <shar_modcars.h>
#endif



//------------------------------------------------------------------------
SuspensionJointDriver::SuspensionJointDriver(Wheel* wheel, int jointIndex)
{
    mWheel = wheel;
    mJointIndex = jointIndex;

}


//------------------------------------------------------------------------
SuspensionJointDriver::~SuspensionJointDriver()
{
    //
}



//------------------------------------------------------------------------
void SuspensionJointDriver::Update(poser::Pose* pose)
{
    // process results of last time's collision detection and solving...


    //mWheel->ResolveOffset();    // I think this call is redundant - bug

    // now just use mYOffset
    //float yOffset = mWheel->mYOffset;   // need to reapply this because we called PoseEngine::Begin and overwrote the suspension results...


    //!
    // BUT
    // 
    // this mYOffset is from the bottomed out location set in precollisionprep, not the neutral point
    //
    // try not calling PoseEngine::Begin in PostUpdate







    poser::Joint* joint = pose->GetJoint(mJointIndex);

    // TODO - is this call inefficient?
    // I'm gonna have to make a copy of the matrix at some point so I can change some values....
    rmt::Matrix matrix = joint->GetObjectMatrix();




//Matrix& FillRotateXYZ(float anglex, float angley, float anglez);

    /*


    rmt::Matrix rot;
    rot.Identity();
    rot.FillRotateX(mWheel->mRotAngle * 100.0f);

    matrix.Mult(rot);


    */

    
    //matrix.m[3][1] += yOffset;

    // !! TODO - safe to just apply this?
    //matrix.m[3][1] += mWheel->mPhysicsVehicleOwner->mGravitySettleYCorrection;  // this is insignificant

#ifdef RAD_LINUX
    // CustomCarSupport steering visual multipliers: any wheel turns by its share of the steering angle (the front
    // wheels' own angle: what the game draws them with)
    const SharModCar* modCar = mWheel->mVehicle->mModCar;
    if( modCar != NULL )
    {
        float turn = mWheel->mVehicle->mWheels[ 2 ]->mWheelTurnAngle * shar_ModCarSteering( modCar, mWheel->mWheelNum );
        if( turn != 0.0f )
        {
            matrix.FillRotateXYZ( mWheel->mCumulativeRot, turn, 0.0f );
        }
        else
        {
            matrix.FillRotateX( mWheel->mCumulativeRot );
        }
    }
    else
#endif
    if(mWheel->mSteerWheel)
    {
        // this is one of the two front wheels, so turn it
        float angle = mWheel->mWheelTurnAngle;

        // should this be done before the addition of the yOffset?
        // shouldn't matter too much....
        matrix.FillRotateXYZ(mWheel->mCumulativeRot, mWheel->mWheelTurnAngle, 0.0f);

    }
    else
    {
        matrix.FillRotateX(mWheel->mCumulativeRot);
    }

    joint->SetObjectMatrix(matrix);

    /*

    rmt::Vector trans = joint->GetObjectTranslation();

    trans.y += yOffset;
    
    joint->SetObjectTranslation(trans);

    if(mWheel->GetNum() < 1)
    {
        // this is one of the two front wheels, so rotate it

           const rmt::Matrix& GetObjectMatrix() const
        { return GetObjectTransform().GetMatrix(); }
    void SetObjectMatrix(const rmt::Matrix& matrix);
  

    }
    */
}

#ifdef RAD_LINUX
//------------------------------------------------------------------------
FakeWheelJointDriver::FakeWheelJointDriver( Vehicle* vehicle, const SharModFakeWheel* config, int jointIndex,
                                            const rmt::Vector& restPoint ) :
    mVehicle( vehicle ),
    mConfig( config ),
    mJointIndex( jointIndex ),
    mRestPoint( restPoint )
{
}

//------------------------------------------------------------------------
void FakeWheelJointDriver::Update( poser::Pose* pose )
{
    float rotation = 0.0f;
    float travel = 0.0f;
    bool rides = false;
    for( int i = 0; i < 4; i++ )
    {
        if( mConfig->copyRotation & ( 1 << i ) )
        {
            rotation += mVehicle->mWheels[ i ]->mCumulativeRot;
        }
        if( mConfig->copySuspension & ( 1 << i ) )
        {
            // the wheel joint's travel from its place in the file this frame; the highest wheel counts
            poser::Joint* wheelJoint = pose->GetJoint( mVehicle->mWheelToJointIndexMapping[ i ] );
            float y = wheelJoint->GetObjectTranslation().y - mVehicle->mSuspensionRestPointsFromFile[ i ].y;
            if( !rides || y > travel )
            {
                travel = y;
            }
            rides = true;
        }
    }
    float turn = mVehicle->mWheels[ 2 ]->mWheelTurnAngle * mConfig->steeringMultiplier;

    poser::Joint* joint = pose->GetJoint( mJointIndex );
    rmt::Matrix matrix = joint->GetObjectMatrix();
    matrix.FillRotateXYZ( rotation, turn, 0.0f );
    rmt::Vector trans = mRestPoint;
    trans.y += travel;
    matrix.FillTranslate( trans );
    joint->SetObjectMatrix( matrix );
}
#endif

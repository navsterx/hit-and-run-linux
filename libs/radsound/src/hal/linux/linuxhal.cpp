//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// Linux port: radsound hardware abstraction layer with a software mixer on SDL2 audio.
//
// Replaces the DirectSound HAL in hal/win32:
//   - buffers keep their PCM in the radsound memory object (loaded through the shared buffer loader)
//   - an SDL audio callback mixes every playing voice (linear resampling for sample rate x pitch,
//     volume/trim/mute as amplitude = analog^2 like DirectSound, DirectSound pan law) and owns the play
//     position, so streams that refill a looping buffer by watching the position stay in step with what
//     is heard; looping buffers wrap, one-shots stop and rewind at the end
//   - 3D positioning uses radsound's own software listener / positional group (hal/common): its volume,
//     pan and pitch (doppler) adjustments are applied by the mixer
//   - if no audio device can be opened (or SHAR_NO_AUDIO is set) voices advance in real time without
//     output, so everything that waits on sounds still behaves
// Voice state shared with the callback (play list, buffer, position) changes under the audio device lock.
//=============================================================================

#include "pch.hpp"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <radlinkedclass.hpp>
#include <radsound_hal.hpp>
#include <radsound_win32.hpp>
#include <radsoundobject.hpp>
#include <radsoundupdatableobject.hpp>
#include <radtime.hpp>
#include "../common/banner.hpp"
#include "../common/memoryregion.hpp"
#include "../common/softwarelistener.hpp"
#include "../common/softwarepositionalgroup.hpp"
#include "../win32/bufferloader.hpp"

#define RSD_LINUX_MAX_AUX_SENDS 4

//=============================================================================
// Buffer
//=============================================================================

class radSoundHalBufferLinux
    :
    public IRadSoundHalBuffer,
    public IRadSoundHalBufferLoadCallback,
    public radLinkedClass< radSoundHalBufferLinux >,
    public radSoundObject
{
public:
    IMPLEMENT_REFCOUNTED( "radSoundHalBufferLinux" )

    radSoundHalBufferLinux( void ) : m_SizeInFrames( 0 ), m_Looping( false ), m_Streaming( false ) { }

    virtual void Initialize( IRadSoundHalAudioFormat* pFormat, IRadMemoryObject* pMemory, unsigned int sizeInFrames,
                             bool looping, bool streaming )
    {
        rAssert( pFormat != NULL );
        rAssert( pFormat->GetEncoding( ) == IRadSoundHalAudioFormat::PCM );
        m_refFormat = pFormat;
        m_refMemory = pMemory;
        m_SizeInFrames = sizeInFrames;
        m_Looping = looping;
        m_Streaming = streaming;
    }

    virtual IRadSoundHalAudioFormat* GetFormat( void ) { return m_refFormat; }
    virtual IRadMemoryObject* GetMemoryObject( void ) { return m_refMemory; }
    virtual bool IsLooping( void ) { return m_Looping; }
    virtual unsigned int GetSizeInFrames( void ) { return m_SizeInFrames; }

    virtual void LoadAsync( IRadSoundHalDataSource* pDataSource, unsigned int bufferStartInFrames,
                            unsigned int numberOfFrames, IRadSoundHalBufferLoadCallback* pCallback )
    {
        rAssert( m_refLoadCallback == NULL );
        m_refLoadCallback = pCallback;
        char* dest = (char*) m_refMemory->GetMemoryAddress( ) + m_refFormat->FramesToBytes( bufferStartInFrames );
        new( "radSoundBufferLoaderLinux", RADMEMORY_ALLOC_TEMP ) radSoundBufferLoaderWin(
            static_cast< IRadSoundHalBuffer* >( this ), dest, pDataSource, m_refFormat, numberOfFrames, this );
    }

    virtual void OnBufferLoadComplete( unsigned int dataSourceFrames )
    {
        if ( m_refLoadCallback != NULL )
        {
            ref< IRadSoundHalBufferLoadCallback > cb = m_refLoadCallback;
            m_refLoadCallback = NULL;
            cb->OnBufferLoadComplete( dataSourceFrames );
        }
    }

    virtual void ClearAsync( unsigned int startPositionInFrames, unsigned int numberOfFrames,
                             IRadSoundHalBufferClearCallback* pCallback )
    {
        unsigned char fill = ( m_refFormat->GetBitResolution( ) == 8 ) ? PCM_SILENCE_8_BIT : 0;
        memset( (char*) m_refMemory->GetMemoryAddress( ) + m_refFormat->FramesToBytes( startPositionInFrames ),
                fill, m_refFormat->FramesToBytes( numberOfFrames ) );
        if ( pCallback != NULL )
        {
            pCallback->OnBufferClearComplete( );
        }
    }

    virtual unsigned int GetMinTransferSize( IRadSoundHalAudioFormat::SizeType sizeType )
    {
        return m_refFormat->ConvertSizeType( sizeType,
            radMemorySpace_OptimalMultiple * m_refFormat->GetNumberOfChannels( ), IRadSoundHalAudioFormat::Bytes );
    }

    virtual void CancelAsyncOperations( void )
    {
        radSoundBufferLoaderWin::CancelOperations( static_cast< IRadSoundHalBuffer* >( this ) );
        m_refLoadCallback = NULL;
    }

    virtual void ReSetAudioFormat( IRadSoundHalAudioFormat* pFormat ) { }

    unsigned int GetSizeInBytes( void ) { return m_refFormat != NULL ? m_refFormat->FramesToBytes( m_SizeInFrames ) : 0; }

private:
    virtual ~radSoundHalBufferLinux( void ) { CancelAsyncOperations( ); }

    unsigned int m_SizeInFrames;
    bool m_Looping;
    bool m_Streaming;
    ref< IRadSoundHalAudioFormat >          m_refFormat;
    ref< IRadMemoryObject >                 m_refMemory;
    ref< IRadSoundHalBufferLoadCallback >   m_refLoadCallback;
};

template<> radSoundHalBufferLinux* radLinkedClass< radSoundHalBufferLinux >::s_pLinkedClassHead = NULL;
template<> radSoundHalBufferLinux* radLinkedClass< radSoundHalBufferLinux >::s_pLinkedClassTail = NULL;

IRadSoundHalBuffer* radSoundHalBufferCreate( radMemoryAllocator allocator )
{
    return new( "radSoundHalBufferLinux", allocator ) radSoundHalBufferLinux( );
}

//=============================================================================
// Mixer state (audio device + list of playing voices, both guarded by the device lock)
//=============================================================================

class radSoundHalVoiceLinux;

static SDL_AudioDeviceID s_AudioDevice = 0;
static int s_OutputRate = 48000;
static const unsigned int MAX_ACTIVE_VOICES = 256;
static radSoundHalVoiceLinux* s_ActiveVoices[ MAX_ACTIVE_VOICES ];
static unsigned int s_NumActiveVoices = 0;

struct radSoundMixLock
{
    radSoundMixLock( void ) { if ( s_AudioDevice != 0 ) SDL_LockAudioDevice( s_AudioDevice ); }
    ~radSoundMixLock( void ) { if ( s_AudioDevice != 0 ) SDL_UnlockAudioDevice( s_AudioDevice ); }
};

//=============================================================================
// Movie stream: radmovie's FFmpeg Bink player pushes decoded stereo float audio (at the output rate) into
// this ring; the mixer plays it while the stream is running. Stands in for Bink's own DirectSound output.
//=============================================================================

static float* s_MovieRing = NULL;               // interleaved stereo
static unsigned int s_MovieRingFrames = 0;
static unsigned int s_MovieRead = 0;            // frame indices, wrap at s_MovieRingFrames
static unsigned int s_MovieFill = 0;            // frames buffered
static bool s_MovieRunning = false;
static float s_MovieGain[ 2 ] = { 1.0f, 1.0f };

void radSoundLinuxMovieStreamOpen( unsigned int seconds )
{
    radSoundMixLock lock;
    free( s_MovieRing );
    s_MovieRingFrames = seconds * s_OutputRate;
    s_MovieRing = (float*) malloc( s_MovieRingFrames * 2 * sizeof( float ) );
    s_MovieRead = s_MovieFill = 0;
    s_MovieRunning = false;
}

void radSoundLinuxMovieStreamClose( void )
{
    radSoundMixLock lock;
    free( s_MovieRing );
    s_MovieRing = NULL;
    s_MovieRingFrames = s_MovieRead = s_MovieFill = 0;
    s_MovieRunning = false;
}

unsigned int radSoundLinuxMovieStreamOutputRate( void ) { return s_OutputRate; }

// Returns the frames accepted (the rest is dropped when the ring is full).
unsigned int radSoundLinuxMovieStreamWrite( const float* stereo, unsigned int frames )
{
    radSoundMixLock lock;
    if ( s_MovieRing == NULL || s_AudioDevice == 0 )
    {
        return frames;
    }
    unsigned int space = s_MovieRingFrames - s_MovieFill;
    if ( frames > space ) frames = space;
    unsigned int w = ( s_MovieRead + s_MovieFill ) % s_MovieRingFrames;
    for ( unsigned int i = 0; i < frames; i++ )
    {
        s_MovieRing[ w * 2 ] = stereo[ i * 2 ];
        s_MovieRing[ w * 2 + 1 ] = stereo[ i * 2 + 1 ];
        if ( ++w == s_MovieRingFrames ) w = 0;
    }
    s_MovieFill += frames;
    return frames;
}

void radSoundLinuxMovieStreamSetRunning( bool running )
{
    radSoundMixLock lock;
    s_MovieRunning = running;
}

void radSoundLinuxMovieStreamSetGains( float left, float right )
{
    radSoundMixLock lock;
    s_MovieGain[ 0 ] = left;
    s_MovieGain[ 1 ] = right;
}

static void radSoundMixMovie( float* out, int frames )
{
    if ( !s_MovieRunning || s_MovieRing == NULL )
    {
        return;
    }
    for ( int i = 0; i < frames && s_MovieFill > 0; i++ )
    {
        out[ i * 2 ] += s_MovieRing[ s_MovieRead * 2 ] * s_MovieGain[ 0 ];
        out[ i * 2 + 1 ] += s_MovieRing[ s_MovieRead * 2 + 1 ] * s_MovieGain[ 1 ];
        if ( ++s_MovieRead == s_MovieRingFrames ) s_MovieRead = 0;
        s_MovieFill--;
    }
}

//=============================================================================
// Voice
//=============================================================================

class radSoundHalVoiceLinux
    :
    public IRadSoundHalVoice,
    public radSoundHalPositionalEntity,
    public radSoundUpdatableObject,
    public radLinkedClass< radSoundHalVoiceLinux >
{
public:
    IMPLEMENT_REFCOUNTED( "radSoundHalVoiceLinux" )

    radSoundHalVoiceLinux( void )
        :
        m_Priority( 0 ), m_Playing( false ), m_InUpdateList( false ), m_ActiveIndex( -1 ), m_PositionFrames( 0.0 ),
        m_Volume( 1.0f ), m_Trim( 1.0f ), m_Muted( false ), m_Pan( 0.0f ), m_Pitch( 1.0f ),
        m_LastGainL( 0.0f ), m_LastGainR( 0.0f )
    {
        for ( unsigned int i = 0; i < RSD_LINUX_MAX_AUX_SENDS; i++ )
        {
            m_AuxMode[ i ] = radSoundAuxMode_PostFader;
            m_AuxGain[ i ] = 1.0f;
        }
    }

    virtual void SetPriority( unsigned int priority ) { m_Priority = priority; }
    virtual unsigned int GetPriority( void ) { return m_Priority; }

    virtual void SetBuffer( IRadSoundHalBuffer* pBuffer )
    {
        Stop( );
        radSoundMixLock lock;
        m_refBuffer = static_cast< radSoundHalBufferLinux* >( pBuffer );
        m_PositionFrames = 0.0;
    }
    virtual IRadSoundHalBuffer* GetBuffer( void ) { return m_refBuffer; }

    virtual void Play( void )
    {
        if ( m_refBuffer == NULL || m_Playing ) return;
        {
            radSoundMixLock lock;
            if ( s_NumActiveVoices >= MAX_ACTIVE_VOICES ) return;
            m_ActiveIndex = s_NumActiveVoices;
            s_ActiveVoices[ s_NumActiveVoices++ ] = this;
            m_LastGainL = m_LastGainR = -1.0f;          // first mixed block starts at the target gain
            m_Playing = true;
        }
        if ( !m_InUpdateList )
        {
            AddToUpdateList( );
            m_InUpdateList = true;
        }
    }

    virtual void Stop( void )
    {
        {
            radSoundMixLock lock;
            if ( m_Playing ) Deactivate( );
        }
        if ( m_InUpdateList )
        {
            RemoveFromUpdateList( );
            m_InUpdateList = false;
        }
    }

    virtual bool IsPlaying( void ) { return m_Playing; }

    virtual void SetPlaybackPositionInSamples( unsigned int position )
    {
        radSoundMixLock lock;
        if ( m_refBuffer != NULL )
            m_PositionFrames = m_refBuffer->GetFormat( )->SamplesToFrames( position );
    }
    virtual unsigned int GetPlaybackPositionInSamples( void )
    {
        return m_refBuffer != NULL ? m_refBuffer->GetFormat( )->FramesToSamples( (unsigned int) m_PositionFrames ) : 0;
    }

    virtual void SetVolume( float volume ) { m_Volume = volume; }
    virtual float GetVolume( void ) { return m_Volume; }
    virtual void SetTrim( float trim ) { m_Trim = trim; }
    virtual float GetTrim( void ) { return m_Trim; }
    virtual void SetMuted( bool muteOn ) { m_Muted = muteOn; }
    virtual bool GetMuted( void ) { return m_Muted; }
    virtual void SetPan( float pan ) { m_Pan = pan; }
    virtual float GetPan( void ) { return m_Pan; }
    virtual void SetPitch( float pitch ) { m_Pitch = pitch; }
    virtual float GetPitch( void ) { return m_Pitch; }

    virtual void SetAuxMode( unsigned int aux, radSoundAuxMode mode ) { if ( aux < RSD_LINUX_MAX_AUX_SENDS ) m_AuxMode[ aux ] = mode; }
    virtual radSoundAuxMode GetAuxMode( unsigned int aux ) { return aux < RSD_LINUX_MAX_AUX_SENDS ? m_AuxMode[ aux ] : radSoundAuxMode_PostFader; }
    virtual void SetAuxGain( unsigned int aux, float gain ) { if ( aux < RSD_LINUX_MAX_AUX_SENDS ) m_AuxGain[ aux ] = gain; }
    virtual float GetAuxGain( unsigned int aux ) { return aux < RSD_LINUX_MAX_AUX_SENDS ? m_AuxGain[ aux ] : 0.0f; }

    virtual void SetPositionalGroup( IRadSoundHalPositionalGroup* pGroup )
    {
        radSoundMixLock lock;
        if ( m_xPositionalGroup != NULL ) m_xPositionalGroup->RemovePositionalEntity( this );
        m_xPositionalGroup = static_cast< radSoundHalPositionalGroup* >( pGroup );
        if ( m_xPositionalGroup != NULL ) m_xPositionalGroup->AddPositionalEntity( this );
    }
    virtual IRadSoundHalPositionalGroup* GetPositionalGroup( void ) { return m_xPositionalGroup; }

    // radSoundHalPositionalEntity: the group recomputed its volume/pan/pitch adjustments (used by a mixer)
    virtual void OnApplyPositionalSettings( void ) { }

private:
    virtual ~radSoundHalVoiceLinux( void )
    {
        Stop( );
        if ( m_xPositionalGroup != NULL ) m_xPositionalGroup->RemovePositionalEntity( this );
    }

    // radSoundUpdatableObject: called from IRadSoundHalSystem::Service with the elapsed milliseconds.
    // With an audio device the mixer advances the position; this only drops voices it has finished.
    virtual void Update( unsigned int elapsed )
    {
        if ( !m_Playing )
        {
            if ( m_InUpdateList )
            {
                RemoveFromUpdateList( );
                m_InUpdateList = false;
            }
            return;
        }
        if ( s_AudioDevice != 0 || m_refBuffer == NULL ) return;

        // no audio output: advance in real time
        IRadSoundHalAudioFormat* f = m_refBuffer->GetFormat( );
        unsigned int size = m_refBuffer->GetSizeInFrames( );
        if ( f == NULL || size == 0 ) { Stop( ); return; }
        double framesPerMs = (double) f->GetSampleRate( ) / (double) f->GetFrameSizeInSamples( ) / 1000.0;
        m_PositionFrames += framesPerMs * EffectivePitch( ) * (double) elapsed;
        if ( m_PositionFrames >= (double) size )
        {
            if ( m_refBuffer->IsLooping( ) )
            {
                while ( m_PositionFrames >= (double) size ) m_PositionFrames -= (double) size;
            }
            else
            {
                m_PositionFrames = 0.0;               // DirectSound rewinds a finished one-shot buffer
                Stop( );
            }
        }
    }

    float EffectivePitch( void )
    {
        float pitch = m_Pitch;
        if ( m_xPositionalGroup != NULL )
            pitch *= m_xPositionalGroup->m_RadSoundHalPositionalInformation.m_PitchAdjust;
        return pitch;
    }

    // remove from the mixer's play list (device lock held)
    void Deactivate( void )
    {
        m_Playing = false;
        if ( m_ActiveIndex >= 0 )
        {
            radSoundHalVoiceLinux* last = s_ActiveVoices[ --s_NumActiveVoices ];
            s_ActiveVoices[ m_ActiveIndex ] = last;
            last->m_ActiveIndex = m_ActiveIndex;
            m_ActiveIndex = -1;
        }
    }

public:
    // Mix this voice into out (interleaved stereo float, frames output frames). Audio thread, lock held.
    // Returns false when a one-shot finished.
    bool MixInto( float* out, int frames )
    {
        radSoundHalBufferLinux* b = m_refBuffer;
        if ( b == NULL ) return false;
        IRadSoundHalAudioFormat* f = b->GetFormat( );
        unsigned int size = b->GetSizeInFrames( );
        if ( f == NULL || size == 0 || b->GetMemoryObject( ) == NULL ) return false;
        const unsigned char* data = (const unsigned char*) b->GetMemoryObject( )->GetMemoryAddress( );
        int channels = f->GetNumberOfChannels( );
        int bits = f->GetBitResolution( );
        bool looping = b->IsLooping( );

        // gains: volume/trim are analog (amplitude = analog^2), positional rolloff is an amplitude
        float analog = m_Muted ? 0.0f : m_Volume * m_Trim;
        if ( analog < 0.0f ) analog = 0.0f;
        float amp = analog * analog;
        float pan = m_Pan;
        if ( m_xPositionalGroup != NULL )
        {
            radSoundHalPositionalInformation& pi = m_xPositionalGroup->m_RadSoundHalPositionalInformation;
            amp *= pi.m_VolumeAdjust;
            pan = pi.m_PanAdjust * 0.75f;               // keep some of a fully-sided source in the far ear
        }
        if ( pan > 1.0f ) pan = 1.0f; else if ( pan < -1.0f ) pan = -1.0f;
        // DirectSound pan law: attenuate the opposite side (analog), centre = full volume in both
        float l = pan > 0.0f ? ( 1.0f - pan ) : 1.0f;
        float r = pan < 0.0f ? ( 1.0f + pan ) : 1.0f;
        float gainL = amp * l * l, gainR = amp * r * r;
        if ( m_LastGainL < 0.0f ) { m_LastGainL = gainL; m_LastGainR = gainR; }
        float dL = ( gainL - m_LastGainL ) / frames, dR = ( gainR - m_LastGainR ) / frames;
        float gL = m_LastGainL, gR = m_LastGainR;
        m_LastGainL = gainL; m_LastGainR = gainR;

        double step = (double) f->GetSampleRate( ) * EffectivePitch( ) / (double) s_OutputRate;
        // a NaN from the game (e.g. a positional source at a NaN position) would poison the whole mix
        if ( !( step > 0.0 && step < 64.0 ) || !( gainL >= 0.0f && gainL < 16.0f ) || !( gainR >= 0.0f && gainR < 16.0f ) )
        {
            m_LastGainL = m_LastGainR = -1.0f;
            return true;
        }
        double pos = m_PositionFrames;
        bool finished = false;

        for ( int i = 0; i < frames; i++ )
        {
            if ( pos >= (double) size )
            {
                if ( looping ) { while ( pos >= (double) size ) pos -= (double) size; }
                else { finished = true; break; }
            }
            unsigned int i0 = (unsigned int) pos;
            unsigned int i1 = i0 + 1;
            if ( i1 >= size ) i1 = looping ? 0 : i0;
            float t = (float) ( pos - (double) i0 );
            float sl, sr;
            if ( bits == 16 )
            {
                const short* p = (const short*) data;
                float a0 = p[ i0 * channels ], a1 = p[ i1 * channels ];
                sl = ( a0 + ( a1 - a0 ) * t ) * ( 1.0f / 32768.0f );
                if ( channels > 1 )
                {
                    float b0 = p[ i0 * channels + 1 ], b1 = p[ i1 * channels + 1 ];
                    sr = ( b0 + ( b1 - b0 ) * t ) * ( 1.0f / 32768.0f );
                }
                else sr = sl;
            }
            else
            {
                float a0 = (float) data[ i0 * channels ] - 128.0f, a1 = (float) data[ i1 * channels ] - 128.0f;
                sl = ( a0 + ( a1 - a0 ) * t ) * ( 1.0f / 128.0f );
                if ( channels > 1 )
                {
                    float b0 = (float) data[ i0 * channels + 1 ] - 128.0f, b1 = (float) data[ i1 * channels + 1 ] - 128.0f;
                    sr = ( b0 + ( b1 - b0 ) * t ) * ( 1.0f / 128.0f );
                }
                else sr = sl;
            }
            out[ i * 2 ]     += sl * gL;
            out[ i * 2 + 1 ] += sr * gR;
            gL += dL; gR += dR;
            pos += step;
        }

        if ( finished )
        {
            m_PositionFrames = 0.0;                     // DirectSound rewinds a finished one-shot buffer
            return false;
        }
        if ( looping ) { while ( pos >= (double) size ) pos -= (double) size; }   // never report pos == size
        else if ( pos >= (double) size ) { m_PositionFrames = 0.0; return false; }
        m_PositionFrames = pos;
        return true;
    }

    // Audio thread: finish a one-shot (lock held). The update list entry is dropped by Update.
    void FinishFromMixer( void ) { Deactivate( ); }

private:
    unsigned int m_Priority;
    volatile bool m_Playing;
    bool   m_InUpdateList;                  // main thread only
    int    m_ActiveIndex;                   // slot in s_ActiveVoices, -1 when not playing
    volatile double m_PositionFrames;
    float  m_LastGainL, m_LastGainR;        // gains at the end of the last mixed block (ramped)
    float  m_Volume, m_Trim;
    bool   m_Muted;
    float  m_Pan, m_Pitch;
    radSoundAuxMode m_AuxMode[ RSD_LINUX_MAX_AUX_SENDS ];
    float  m_AuxGain[ RSD_LINUX_MAX_AUX_SENDS ];
    ref< radSoundHalBufferLinux > m_refBuffer;
    ref< radSoundHalPositionalGroup > m_xPositionalGroup;
};

template<> radSoundHalVoiceLinux* radLinkedClass< radSoundHalVoiceLinux >::s_pLinkedClassHead = NULL;
template<> radSoundHalVoiceLinux* radLinkedClass< radSoundHalVoiceLinux >::s_pLinkedClassTail = NULL;

IRadSoundHalVoice* radSoundHalVoiceCreate( radMemoryAllocator allocator )
{
    return new( "radSoundHalVoiceLinux", allocator ) radSoundHalVoiceLinux( );
}

//=============================================================================
// EAX2 reverb effect (parameters are stored; the mixer has no reverb)
//=============================================================================

class radSoundHalEffectEAX2ReverbLinux : public IRadSoundHalEffectEAX2Reverb, public radSoundObject
{
public:
    IMPLEMENT_REFCOUNTED( "radSoundHalEffectEAX2ReverbLinux" )

    radSoundHalEffectEAX2ReverbLinux( void )
        : m_AuxSend( 0xFFFFFFFF ), m_Room( -1000 ), m_RoomHF( -100 ), m_RoomRolloff( 0.0f ), m_DecayTime( 1.49f ),
          m_DecayHFRatio( 0.83f ), m_Reflections( -2602 ), m_ReflectionsDelay( 0.007f ), m_Reverb( 200 ),
          m_ReverbDelay( 0.011f ), m_Diffusion( 1.0f ), m_AirAbsorptionHF( -5.0f ) { }

    virtual void Attach( unsigned int auxSend ) { m_AuxSend = auxSend; }
    virtual void Detach( void ) { m_AuxSend = 0xFFFFFFFF; }
    virtual void Update( void ) { }

    virtual void  SetRoom( int v ) { m_Room = v; }                     virtual int   GetRoom( void ) { return m_Room; }
    virtual void  SetRoomHF( int v ) { m_RoomHF = v; }                 virtual int   GetRoomHF( void ) { return m_RoomHF; }
    virtual void  SetRoomRolloffFactor( float v ) { m_RoomRolloff = v; } virtual float GetRoomRolloffFactor( void ) { return m_RoomRolloff; }
    virtual void  SetDecayTime( float v ) { m_DecayTime = v; }         virtual float GetDecayTime( void ) { return m_DecayTime; }
    virtual void  SetDecayHFRatio( float v ) { m_DecayHFRatio = v; }   virtual float GetDecayHFRatio( void ) { return m_DecayHFRatio; }
    virtual void  SetReflections( int v ) { m_Reflections = v; }       virtual int   GetReflections( void ) { return m_Reflections; }
    virtual void  SetReflectionsDelay( float v ) { m_ReflectionsDelay = v; } virtual float GetReflectionsDelay( void ) { return m_ReflectionsDelay; }
    virtual void  SetReverb( int v ) { m_Reverb = v; }                 virtual int   GetReverb( void ) { return m_Reverb; }
    virtual void  SetReverbDelay( float v ) { m_ReverbDelay = v; }     virtual float GetReverbDelay( void ) { return m_ReverbDelay; }
    virtual void  SetEnvironmentDiffusion( float v ) { m_Diffusion = v; } virtual float GetEnvironmentDiffusion( void ) { return m_Diffusion; }
    virtual void  SetAirAbsorptionHF( float v ) { m_AirAbsorptionHF = v; } virtual float GetAirAbsorptionHF( void ) { return m_AirAbsorptionHF; }

private:
    unsigned int m_AuxSend;
    int m_Room, m_RoomHF;
    float m_RoomRolloff, m_DecayTime, m_DecayHFRatio;
    int m_Reflections;
    float m_ReflectionsDelay;
    int m_Reverb;
    float m_ReverbDelay, m_Diffusion, m_AirAbsorptionHF;
};

IRadSoundHalEffectEAX2Reverb* radSoundHalEffectEAX2ReverbCreate( radMemoryAllocator allocator )
{
    return new( "radSoundHalEffectEAX2ReverbLinux", allocator ) radSoundHalEffectEAX2ReverbLinux( );
}

//=============================================================================
// System
//=============================================================================

static void SDLCALL radSoundMixCallback( void* /*user*/, Uint8* stream, int len )
{
    float* out = (float*) stream;
    int frames = len / ( 2 * sizeof( float ) );
    memset( stream, 0, len );
    for ( unsigned int i = 0; i < s_NumActiveVoices; )
    {
        radSoundHalVoiceLinux* v = s_ActiveVoices[ i ];
        if ( v->MixInto( out, frames ) )
        {
            i++;
        }
        else
        {
            v->FinishFromMixer( );                      // swaps the last voice into slot i
        }
    }
    radSoundMixMovie( out, frames );
    for ( int i = 0; i < frames * 2; i++ )             // soft limit instead of hard clipping
    {
        float x = out[ i ];
        if ( x > 1.0f || x < -1.0f ) out[ i ] = x / ( 1.0f + ( ( x > 0 ? x : -x ) - 1.0f ) );
    }
}

static void radSoundOpenAudio( void )
{
    if ( getenv( "SHAR_NO_AUDIO" ) != NULL )
    {
        fprintf( stderr, "radsound: SHAR_NO_AUDIO set, running silent\n" );
        return;
    }
    if ( SDL_InitSubSystem( SDL_INIT_AUDIO ) != 0 )
    {
        fprintf( stderr, "radsound: SDL audio init failed (%s), running silent\n", SDL_GetError( ) );
        return;
    }
    SDL_AudioSpec want, have;
    memset( &want, 0, sizeof( want ) );
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = radSoundMixCallback;
    s_AudioDevice = SDL_OpenAudioDevice( NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE );
    if ( s_AudioDevice == 0 )
    {
        fprintf( stderr, "radsound: no audio device (%s), running silent\n", SDL_GetError( ) );
        return;
    }
    s_OutputRate = have.freq;
    fprintf( stderr, "radsound: SDL audio driver %s, %d Hz, %d-frame buffer\n",
             SDL_GetCurrentAudioDriver( ), have.freq, have.samples );
    SDL_PauseAudioDevice( s_AudioDevice, 0 );
}

static void radSoundCloseAudio( void )
{
    if ( s_AudioDevice != 0 )
    {
        SDL_CloseAudioDevice( s_AudioDevice );
        s_AudioDevice = 0;
        SDL_QuitSubSystem( SDL_INIT_AUDIO );
    }
}

class radSoundHalSystemLinux : public IRadSoundHalSystem, public radSoundObject
{
public:
    IMPLEMENT_REFCOUNTED( "radSoundHalSystemLinux" )

    static radSoundHalSystemLinux* s_pSystem;

    radSoundHalSystemLinux( void )
        : m_NumAuxSends( 0 ), m_pSoundMemory( NULL ), m_LastServiceTime( ::radTimeGetMilliseconds( ) )
    {
        s_pSystem = this;
        for ( unsigned int i = 0; i < RSD_LINUX_MAX_AUX_SENDS; i++ )
        {
            m_AuxGain[ i ] = 1.0f;
        }
        ::radSoundPrintBanner( );
    }

    virtual void Initialize( const SystemDescription& desc )
    {
        m_NumAuxSends = desc.m_NumAuxSends < RSD_LINUX_MAX_AUX_SENDS ? desc.m_NumAuxSends : RSD_LINUX_MAX_AUX_SENDS;
        radSoundHalListener::Initialize( GetThisAllocator( ) );
        m_pSoundMemory = ::radMemoryAllocAligned( GetThisAllocator( ), desc.m_ReservedSoundMemory,
                                                  radSoundHalDataSourceReadAlignmentGet( ) );
        radSoundHalMemoryRegion::Initialize( m_pSoundMemory, desc.m_ReservedSoundMemory, desc.m_MaxRootAllocations,
                                             radSoundHalDataSourceReadAlignmentGet( ), radMemorySpace_Local,
                                             GetThisAllocator( ) );
        radSoundOpenAudio( );
    }

    virtual IRadSoundHalMemoryRegion* GetRootMemoryRegion( void ) { return radSoundHalMemoryRegion::GetRootRegion( ); }
    virtual unsigned int GetNumAuxSends( ) { return m_NumAuxSends; }

    virtual void SetAuxEffect( unsigned int aux, IRadSoundHalEffect* pEffect )
    {
        rAssert( aux < m_NumAuxSends );
        if ( m_refEffect[ aux ] != NULL ) m_refEffect[ aux ]->Detach( );
        m_refEffect[ aux ] = pEffect;
        if ( m_refEffect[ aux ] != NULL ) m_refEffect[ aux ]->Attach( aux );
    }
    virtual IRadSoundHalEffect* GetAuxEffect( unsigned int aux ) { return aux < m_NumAuxSends ? (IRadSoundHalEffect*) m_refEffect[ aux ] : NULL; }
    virtual void SetAuxGain( unsigned int aux, float gain ) { if ( aux < RSD_LINUX_MAX_AUX_SENDS ) m_AuxGain[ aux ] = gain; }
    virtual float GetAuxGain( unsigned int aux ) { return aux < RSD_LINUX_MAX_AUX_SENDS ? m_AuxGain[ aux ] : 0.0f; }
    virtual void SetOutputMode( radSoundOutputMode mode ) { }
    virtual radSoundOutputMode GetOutputMode( void ) { return radSoundOutputMode_Stereo; }

    virtual void Service( void )
    {
        unsigned int now = ::radTimeGetMilliseconds( );
        radSoundUpdatableObject::UpdateAll( now - m_LastServiceTime );     // advances playing voices
        m_LastServiceTime = now;
    }

    virtual void ServiceOncePerFrame( void )
    {
        radSoundHalListener::GetInstance( )->UpdatePositionalSettings( );
    }

    virtual void GetStats( IRadSoundHalSystem::Stats* pStats )
    {
        memset( pStats, 0, sizeof( *pStats ) );
        for ( radSoundHalVoiceLinux* v = radSoundHalVoiceLinux::GetLinkedClassHead( ); v; v = v->GetLinkedClassNext( ) )
        {
            bool positional = v->GetPositionalGroup( ) != NULL;
            ( positional ? pStats->m_NumPosVoices : pStats->m_NumVoices )++;
            if ( v->IsPlaying( ) ) ( positional ? pStats->m_NumPosVoicesPlaying : pStats->m_NumVoicesPlaying )++;
        }
        for ( radSoundHalBufferLinux* b = radSoundHalBufferLinux::GetLinkedClassHead( ); b; b = b->GetLinkedClassNext( ) )
        {
            pStats->m_NumBuffers++;
            pStats->m_BufferMemoryUsed += b->GetSizeInBytes( );
        }
        radSoundHalMemoryRegion::GetRootRegion( )->GetStats( &pStats->m_TotalFreeSoundMemory, NULL, NULL, true );
    }

    virtual ~radSoundHalSystemLinux( void )
    {
        radSoundCloseAudio( );
        radSoundHalListener::Terminate( );
        radSoundHalMemoryRegion::Terminate( );
        if ( m_pSoundMemory ) ::radMemoryFreeAligned( GetThisAllocator( ), m_pSoundMemory );
        s_pSystem = NULL;
    }

private:
    unsigned int m_NumAuxSends;
    void* m_pSoundMemory;
    unsigned int m_LastServiceTime;
    float m_AuxGain[ RSD_LINUX_MAX_AUX_SENDS ];
    ref< IRadSoundHalEffect > m_refEffect[ RSD_LINUX_MAX_AUX_SENDS ];
};

radSoundHalSystemLinux* radSoundHalSystemLinux::s_pSystem = NULL;

void radSoundHalSystemInitialize( radMemoryAllocator allocator )
{
    rAssert( radSoundHalSystemLinux::s_pSystem == NULL );
    new( "radSoundHalSystem", allocator ) radSoundHalSystemLinux( );
    radSoundHalSystemLinux::s_pSystem->AddRef( );
}

void radSoundHalSystemTerminate( void )
{
    radRelease( radSoundHalSystemLinux::s_pSystem, NULL );
}

IRadSoundHalSystem* radSoundHalSystemGet( void )
{
    return radSoundHalSystemLinux::s_pSystem;
}

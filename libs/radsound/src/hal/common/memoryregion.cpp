//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#include "pch.hpp"
#include <radmemory.hpp>
#include <radsound.hpp>
#include <radmemorymonitor.hpp>
#include "memoryregion.hpp"

#ifdef RAD_LINUX
#include <stdio.h>
#include <stdlib.h>
// SHAR_SOUND_LOG=1: every allocation in the sound memory regions (Linux port diagnostics); out-of-memory always
static bool radSoundRegionLogOn( void )
{
    static int s_on = -1;
    if ( s_on < 0 ) { const char* e = getenv( "SHAR_SOUND_LOG" ); s_on = ( e && *e && *e != '0' ) ? 1 : 0; }
    return s_on == 1;
}
static void radSoundRegionLog( radSoundHalMemoryRegion* pRegion, bool isRoot, const char* what, const char* name,
                               unsigned int size, bool ok )
{
    if ( ok && !radSoundRegionLogOn( ) ) return;
    unsigned int freeBytes = 0, objects = 0, largest = 0;
    pRegion->GetStats( & freeBytes, & objects, & largest, false );
    fprintf( stderr, "%s: %s %s region %p: %s '%s' %u bytes; now %u free (largest %u), %u objects\n",
             ok ? "soundlog" : "radsound", ok ? "alloc" : "OUT OF MEMORY", isRoot ? "root" : "child", (void*) pRegion,
             what, name ? name : "", size, freeBytes, largest, objects );
}
#endif

/* static */ radMemorySpace radSoundHalMemoryRegion::s_MemorySpace = radMemorySpace_Local;
/* static */ radSoundHalMemoryRegion * radSoundHalMemoryRegion::s_pRadSoundMemoryRegion_Root = NULL;
/* static */ unsigned int radSoundHalMemoryRegion::s_Alignment = 1;

//============================================================================
// radSoundHalMemoryRegion::Initialize
//============================================================================

/* static */ void radSoundHalMemoryRegion::Initialize
(
	void * pStartOfMemory,
	unsigned int rootSize,
    unsigned int maxRootAllocations,
	unsigned int alignment,
    radMemorySpace memorySpace,
	radMemoryAllocator allocator
)
{
	s_Alignment = alignment;
    s_MemorySpace = memorySpace;

	rAssert( s_pRadSoundMemoryRegion_Root == NULL );

	s_pRadSoundMemoryRegion_Root = new ( "radSoundHalMemoryRegion (Root)", allocator ) radSoundHalMemoryRegion( );
	s_pRadSoundMemoryRegion_Root->AddRef( );

	s_pRadSoundMemoryRegion_Root->CreateRoot( pStartOfMemory, rootSize, maxRootAllocations );
}

//============================================================================
// radSoundHalMemoryRegion::Terminate
//============================================================================

/* static */ void radSoundHalMemoryRegion::Terminate( void )
{
	rAssert( s_pRadSoundMemoryRegion_Root != NULL );

	s_pRadSoundMemoryRegion_Root->Release( );
	s_pRadSoundMemoryRegion_Root = NULL;
}

//============================================================================
// radSoundHalMemoryRegion::GetRootRegion
//============================================================================

/* static */ radSoundHalMemoryRegion * radSoundHalMemoryRegion::GetRootRegion( void )
{
	return s_pRadSoundMemoryRegion_Root;
}

//============================================================================
// radSoundHalMemoryRegion::radSoundHalMemoryRegion
//============================================================================

radSoundHalMemoryRegion::radSoundHalMemoryRegion( void )
	:
	m_pMemoryRegion_Next( NULL ),
	m_pMemoryRegion_Prev( NULL ),
	m_pMemoryRegion_ChildHead( NULL )
{
}

//============================================================================
// radSoundHalMemoryRegion::Create
//============================================================================

void radSoundHalMemoryRegion::Create
(
	radSoundHalMemoryRegion * pRadSoundMemoryRegion_Parent,
	IRadMemoryObject * pIRadMemoryObject,
    unsigned int maxAllocations
)
{
	rAssert( pRadSoundMemoryRegion_Parent != NULL );
	
	m_xRadSoundMemoryRegion_Parent = pRadSoundMemoryRegion_Parent;

	rAssert( pIRadMemoryObject != NULL );

	m_xIRadMemoryObject_HeapMemory = pIRadMemoryObject;

	::radMemoryExternalHeapCreate(
		& m_xIRadMemoryHeap,
		m_xIRadMemoryObject_HeapMemory->GetMemoryAddress( ),
		m_xIRadMemoryObject_HeapMemory->GetMemorySize( ),
        s_MemorySpace,
        maxAllocations,
        0,
		GetThisAllocator( )
	);

	m_pMemoryRegion_Next = m_xRadSoundMemoryRegion_Parent->m_pMemoryRegion_ChildHead;

	if ( m_pMemoryRegion_Next != NULL )
	{
		m_pMemoryRegion_Next->m_pMemoryRegion_Prev = this;
	}
	m_pMemoryRegion_Prev = NULL;

	m_xRadSoundMemoryRegion_Parent->m_pMemoryRegion_ChildHead = this;
}

IRadSoundHalMemoryRegion * radSoundHalMemoryRegion::CreateChildRegion
(
	unsigned int size,
    unsigned int maxAllocations,
    const char * pIdentifier
)
{
	radSoundHalMemoryRegion * pRadSoundMemoryRegion = NULL;

	ref< IRadMemoryObject > xIRadMemoryObject = NULL;

    radMemorySetAllocationName( pIdentifier );

    if ( size > 0 )
    {
	    m_xIRadMemoryHeap->GetMemoryObjectAligned( & xIRadMemoryObject, size, s_Alignment );
#ifdef RAD_LINUX
        radSoundRegionLog( this, m_xRadSoundMemoryRegion_Parent == NULL, "child region", pIdentifier, size, xIRadMemoryObject != NULL );
#endif

	    if ( xIRadMemoryObject != NULL )
	    {
            ::radMemoryMonitorIdentifyAllocation(
                xIRadMemoryObject->GetMemoryAddress( ),
                radSoundDebugChannel,
                pIdentifier,
                NULL,
                s_MemorySpace );

		    rAssert( ((unsigned int)xIRadMemoryObject->GetMemoryAddress( ) % s_Alignment ) == 0 );
		    rAssert( xIRadMemoryObject->GetMemorySize( ) == size );

		    pRadSoundMemoryRegion = new ( "radSoundHalMemoryRegion", GetThisAllocator( ) ) radSoundHalMemoryRegion( );

		    pRadSoundMemoryRegion->Create( this, xIRadMemoryObject, maxAllocations );

            ::radMemoryMonitorIdentifySection(
                xIRadMemoryObject->GetMemoryAddress( ),
                pIdentifier,
                s_MemorySpace );
	    }
	    else
	    {
		    rTuneString( "radSound: WARNING - Out of memory creating child region\n" );
	    }
    }

	return pRadSoundMemoryRegion;
}

//============================================================================
// radSoundHalMemoryRegion::CreateRoot
//============================================================================

void radSoundHalMemoryRegion::CreateRoot
(
	void * pStartOfMemory,
	unsigned int sizeInBytes,
    unsigned int maxAllocations
)
{
    ::radMemoryMonitorIdentifyAllocation(
        pStartOfMemory,
        radSoundDebugChannel,
        "Sound Data Root Region",
        NULL,
        s_MemorySpace );

	::radMemoryExternalHeapCreate(
		& m_xIRadMemoryHeap,
		pStartOfMemory,
		sizeInBytes,
        s_MemorySpace,
        maxAllocations,
        0,		
		GetThisAllocator( )
	);

    ::radMemoryMonitorIdentifySection(
        pStartOfMemory,
        "Sound Data Root Region",
        s_MemorySpace ); 
}

//============================================================================
// radSoundHalMemoryRegion::radSoundHalMemoryRegion
//============================================================================

/* virtual */ radSoundHalMemoryRegion::~radSoundHalMemoryRegion( void )
{
	if ( m_xRadSoundMemoryRegion_Parent != NULL )
	{
		if ( m_pMemoryRegion_Prev != NULL )
		{
			m_pMemoryRegion_Prev->m_pMemoryRegion_Next = m_pMemoryRegion_Next;
		}
		else
		{
			m_xRadSoundMemoryRegion_Parent->m_pMemoryRegion_ChildHead = m_pMemoryRegion_Next;
		}

		if ( m_pMemoryRegion_Next != NULL )
		{
			m_pMemoryRegion_Next->m_pMemoryRegion_Prev = m_pMemoryRegion_Prev;
		}
	}
}

//============================================================================
// radSoundHalMemoryRegion::CreateMemoryObject
//============================================================================

/* virtual */ void radSoundHalMemoryRegion::CreateMemoryObject
(
	IRadMemoryObject ** ppIRadMemoryObject,
	unsigned int size,
    const char * pIdentifier
)
{
    radMemorySetAllocationName( pIdentifier );

	m_xIRadMemoryHeap->GetMemoryObjectAligned( ppIRadMemoryObject, size, s_Alignment );
#ifdef RAD_LINUX
    radSoundRegionLog( this, m_xRadSoundMemoryRegion_Parent == NULL, "object", pIdentifier, size, *ppIRadMemoryObject != NULL );
#endif

	if ( *ppIRadMemoryObject != NULL )
	{
        ::radMemoryMonitorIdentifyAllocation(
            (*ppIRadMemoryObject)->GetMemoryAddress( ),
            radSoundDebugChannel,
            pIdentifier,
            NULL,
            s_MemorySpace );

		rAssert( ((unsigned int)(*ppIRadMemoryObject)->GetMemoryAddress( ) % s_Alignment ) == 0 );
		rAssert( (*ppIRadMemoryObject)->GetMemorySize( ) == size );
	}
}

//============================================================================
// radSoundHalMemoryRegion::GetSize
//============================================================================

/* virtual */ unsigned int radSoundHalMemoryRegion::GetSize( void )
{
	return m_xIRadMemoryHeap->GetSize( );
}

//============================================================================
// radSoundHalMemoryRegion::GetParent
//============================================================================

/* virtual */ IRadSoundHalMemoryRegion * radSoundHalMemoryRegion::GetParent( void )
{
	return m_xRadSoundMemoryRegion_Parent;
}
//============================================================================
// radSoundHalMemoryRegion::GetFirstChild
//============================================================================

/* virtual */IRadSoundHalMemoryRegion * radSoundHalMemoryRegion::GetFirstChild( void )
{
	return m_pMemoryRegion_ChildHead;
}

//============================================================================
// radSoundHalMemoryRegion::GetNextChild
//============================================================================

/* virtual */ IRadSoundHalMemoryRegion * radSoundHalMemoryRegion::GetNextChild
(
	IRadSoundHalMemoryRegion * pIRadSoundHalMemoryRegion_LastChild
)
{
	rAssert( pIRadSoundHalMemoryRegion_LastChild != NULL );

	return static_cast< radSoundHalMemoryRegion * >(
		pIRadSoundHalMemoryRegion_LastChild )->m_pMemoryRegion_Next;
}

//============================================================================
// radSoundHalMemoryRegion::GetStats
//============================================================================

/* virtual */ void radSoundHalMemoryRegion::GetStats
(
	unsigned int * pFreeMemory,
	unsigned * pNumObjects,
	unsigned int * pLargestBlock,
	bool recurseChildren
)
{
	if ( pFreeMemory != NULL )
	{
		*pFreeMemory = 0;
	}

	if ( pNumObjects != NULL )
	{
		*pNumObjects = 0;
	}

	if ( pLargestBlock != NULL )
	{
		*pLargestBlock = 0;
	}

	GetStats_Recurse( this, pFreeMemory, pNumObjects, pLargestBlock, recurseChildren );
}

//============================================================================
// radSoundHalMemoryRegion::GetStats_Recurse (static)
//============================================================================

/* static */ void radSoundHalMemoryRegion::GetStats_Recurse
(
	radSoundHalMemoryRegion * pRadSoundMemoryRegion,
	unsigned int * pFreeMemory, unsigned int * pNumObjects,
	unsigned int * pLargestBlock,
	bool recurse
)
{
	unsigned int _freeMemory;
	unsigned int _largestBlock;
	unsigned int _numObjects;

	pRadSoundMemoryRegion->m_xIRadMemoryHeap->GetStatus
	(
		pFreeMemory != NULL ? & _freeMemory : NULL,
		pLargestBlock != NULL ? & _largestBlock : NULL,
		pNumObjects != NULL ? & _numObjects : NULL,
		NULL
	);

	if ( pFreeMemory != NULL )
	{
		*pFreeMemory += _freeMemory;
	}

	if ( pLargestBlock != NULL )
	{
		*pLargestBlock += _largestBlock;
	}

	if ( pNumObjects != NULL )
	{
		*pNumObjects += _numObjects;
	}

	if ( recurse == true )
	{
		radSoundHalMemoryRegion * pRsmr = static_cast< radSoundHalMemoryRegion * >(
			pRadSoundMemoryRegion->GetFirstChild( ) );

		while ( pRsmr != NULL )
		{
			GetStats_Recurse( pRsmr, pFreeMemory, pLargestBlock, pNumObjects, recurse );		

			pRsmr = static_cast< radSoundHalMemoryRegion * >( pRadSoundMemoryRegion->GetNextChild( pRsmr) );
		}
	}
}

//========================================================================
// ::radSoundHalBufferCalculateBufferSize
//========================================================================

unsigned int radSoundHalBufferCalculateMemorySize
( 
    IRadSoundHalAudioFormat::SizeType resultType,
    unsigned int requestedSize, 
    IRadSoundHalAudioFormat::SizeType requestedSizeType,
    IRadSoundHalAudioFormat * pIRadSoundHalAudioFormat
)
{
    rAssert( pIRadSoundHalAudioFormat != NULL );

    unsigned int sizeInBytes = pIRadSoundHalAudioFormat->ConvertSizeType(
        IRadSoundHalAudioFormat::Bytes, requestedSize, requestedSizeType );

    // The size of the buffer is 

    sizeInBytes = ::radMemoryRoundUp( sizeInBytes, 
        radMemorySpace_OptimalMultiple * pIRadSoundHalAudioFormat->GetNumberOfChannels( ) );

    unsigned int result = pIRadSoundHalAudioFormat->ConvertSizeType(
        resultType,
        sizeInBytes,
        IRadSoundHalAudioFormat::Bytes );

    return result;
}

#ifdef RAD_XBOX
    #define OPTIMAL_CD_READ_MULTIPLE ( 1024 * 32 ) // see white papaer
#else
    #define OPTIMAL_CD_READ_MULTIPLE radFileMaxSectorSize
#endif

//========================================================================
// radSoundStreamReadMultipleGet
//========================================================================

unsigned int radSoundHalDataSourceReadAlignmentGet( void )
{
    if ( radFileOptimalMemoryAlignment > radMemorySpace_OptimalAlignment )
    {
        return radFileOptimalMemoryAlignment;
    }

    return radMemorySpace_OptimalAlignment;
}

//========================================================================
// radSoundStreamReadMultipleGet
//========================================================================

unsigned int radSoundHalDataSourceReadMultipleGet( void )
{
	unsigned int mult;
	
    if ( OPTIMAL_CD_READ_MULTIPLE > radMemorySpace_OptimalMultiple )
    {
        mult = OPTIMAL_CD_READ_MULTIPLE;
    }
	else
	{
    	mult = radMemorySpace_OptimalMultiple;
   	}
   	
    #ifdef RAD_GAMECUBE

   	    const unsigned int minAdpcm = 32 * 2 * 2;
   	    
   	    if ( mult < minAdpcm )
   	    {
   		    mult = minAdpcm;
   	    }

    #endif

   	
   	return mult;
}
//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#include "pch.hpp"
#include <radmemory.hpp>

//============================================================================
// ::radMemoryRoundUp
//============================================================================

uintptr_t radMemoryRoundUp( uintptr_t size, unsigned int alignment )
{
	if ( alignment != 0 )
	{
		if ( ( size % alignment ) != 0 )
		{
			return size + ( alignment - ( size % alignment ) );
		}
	}

    return size;
}

//============================================================================
// ::radMemoryRoundDown
//============================================================================

uintptr_t radMemoryRoundDown( uintptr_t size, unsigned alignment )
{
    if ( alignment != 0 )
    {
        return ( size / alignment ) * alignment;
    }

    return size;
}

//============================================================================
// ::radMemoryIsAligned
//============================================================================

bool radMemoryIsAligned( uintptr_t value, unsigned int alignment )
{
    return( value == radMemoryRoundUp( value, alignment ) );
}
// Private heap for the renderer (PDDI). On Windows the renderer was a DLL (pddidx8.dll) with its own C
// runtime, so its allocations never touched the game's fixed-size heaps (and texture data lived in
// Direct3D's managed memory). The statically linked GL renderer gets the same separation: after pddigl is
// built, its operator new/delete references are renamed to these functions (see port/gen_cmake.py).

#include <stdlib.h>

extern "C" void* pddi_heap_new(unsigned int size)
{
    void* p = malloc(size ? size : 1);
    if(!p) abort();
    return p;
}

extern "C" void* pddi_heap_new_array(unsigned int size)  { return pddi_heap_new(size); }
extern "C" void  pddi_heap_delete(void* p)               { free(p); }
extern "C" void  pddi_heap_delete_array(void* p)         { free(p); }

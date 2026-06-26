//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#ifndef _GLDISPLAY_HPP_
#define _GLDISPLAY_HPP_
#include <pddi/pddi.hpp>

#include <sys/time.h>
#include <SDL2/SDL.h>
#include <GL/gl.h>

class pglContext;
class pglWrapper;

class pglDisplay : public pddiDisplay
{
public:
    pglDisplay();
    ~pglDisplay();

    // cross-platform functions
    bool InitDisplay(const pddiDisplayInit*);
    bool InitDisplay(int x, int y, int bpp);

    pddiDisplayInfo* GetDisplayInfo(void);

    int GetHeight(void);
    int GetWidth(void);
    int GetDepth(void);
    pddiDisplayMode GetDisplayMode(void);
    int GetNumColourBuffer(void);
    unsigned GetBufferMask(void);

    unsigned GetFreeTextureMem(void);

    void SwapBuffers();

    unsigned Screenshot(pddiColour* buffer, int nBytes);
    
    pddiSurface* NewSurface(int x, int y);

    // linux specific functions (SDL2): the window handle is an SDL_Window*
    void  SetWindowHandle(void* sdlWindow);
    void* GetWindowHandle();

    // internal functions
    void BeginTiming();
    float EndTiming();

    void SetContext(pglContext* c) {context = c;}
    bool ExtBGRA(void) { return extBGRA;}
    bool CheckExtension(char*);
    bool HasReset(void) { return reset; }

    static unsigned FillDisplayModes(pddiModeInfo*);

    void BeginContext(void);
    void EndContext(void);

    void SetGamma(float r, float g, float b);
    void GetGamma(float* r, float* g, float* b);


private:
    pddiDisplayMode mode;
    int winWidth;
    int winHeight;
    int winBitDepth;

    pglContext* context;

    unsigned short initialGammaRamp[3][256];
    float gammaR,gammaG,gammaB;

    SDL_Window*   window;
    SDL_GLContext glContext;
    
    bool extBGRA;
    bool reset;

    struct timeval beginTime;
};

// hack for now.  Ask Nigel how to fix this

// GL_BGRA_EXT comes from <GL/gl.h> (0x80E1). The old Linux code defined it as GL_RGBA, which uploaded
// BGRA texel data as RGBA and swapped red and blue.

#endif


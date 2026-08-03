//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#include <pddi/gl/gltex.hpp>
#include <pddi/gl/glcon.hpp>

#if defined(RAD_WIN32) && !defined(RAD_LINUX)
    #include <pddi/gl/display_win32/gldisplay.hpp>
    #include <pddi/gl/display_win32/gl.hpp>
#endif

#ifdef RAD_LINUX
    #include <pddi/gl/display_linux/gldisplay.hpp>
    #include <pddi/gl/display_linux/gl.hpp>
#endif

#include <math.h>
#include <pddi/base/debug.hpp>
#include <stdio.h>

#ifdef RAD_LINUX
// Pure3D creates and edits textures on its loader thread (fine with Direct3D 8). Uploads happen lazily in SetGLState
// (render thread); the max texture size is cached at display init; stale GL textures are deleted through
// pglDeleteTexture, which queues them for the render thread where no context is current (glpipeline.cpp).
#include <pddi/gl/glpipeline.hpp>
#include <SDL2/SDL.h>

int g_pglMaxTextureSize = 0;                  // set by pglDisplay::InitDisplay on the render thread
#endif

void pglTexture::SetGLState(void)
{
    if(context->contextID != contextID)
    {
        contextID = context->contextID;
        gltexture = 0;
    }

    if(!gltexture)
    {
        glGenTextures(1,&gltexture);
        glBindTexture(GL_TEXTURE_2D, gltexture);

//      if(nMipMap == 0)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, xSize,
                ySize, 0, lock.native ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE,
                (GLvoid *)bits[0]);
#ifdef RAD_LINUX
            // Linux port: out of video memory the upload fails and the texture drew black for good. Say so once
            // and retry on the next use, so textures appear once memory is freed.
            if(glGetError() == GL_OUT_OF_MEMORY)
            {
                static bool s_reported = false;
                if(!s_reported)
                {
                    fprintf(stderr, "PDDI: out of video memory: textures are missing (close other programs using "
                                    "the GPU or lower the resolution)\n");
                    s_reported = true;
                }
                glDeleteTextures(1, &gltexture);
                gltexture = 0;
            }
#endif
        }
        /*
        else
        {

            int tmpMipMap = nMipMap;
            tmpMipMap--;
            if(tmpMipMap < 0)
                tmpMipMap = 0;

            int i = 0;
            int width = xSize;
            int height = ySize;
            bool bottomed = false;
            bool done = false;

            while(!done)
            {
                char* data = bits[i];
                if(i > tmpMipMap)
                    data = bits[tmpMipMap];

                glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA8, xSize >> i,
                    ySize >> i, 0, lock.native ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE,
                    (GLvoid *)data);

                done = (width == 1) || (height == 1);

                width >>= 1;
                if(width < 1) 
                {
                    width = 1;
                    bottomed = true;
                }

                height >>= 1;
                if(height < 1) 
                {
                    height = 1;
                    bottomed = true;
                }

                i++;
            }
        }
        */
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, gltexture);
    }
}

int fastlog2(int x)
{
    int r = 0;
    int tmp = x;
    while(tmp > 1)
    {
        r++;
        tmp = tmp >> 1;

        if((tmp << r) != x)
            // not power of 2
            return -1;
    }
    return r;
}

bool pglTexture::Create(int x, int y, int bpp, int alphaDepth, int nMip, pddiTextureType ty, pddiTextureUsageHint usageHint)
{
    xSize = x;
    ySize = y;
    nMipMap = nMip;
    type = PDDI_TEXTYPE_RGB;

    log2X = fastlog2(xSize);
    log2Y = fastlog2(ySize);

    if((log2X == -1) || (log2Y == -1))
    {
#ifdef RAD_LINUX
        fprintf(stderr, "pglTexture: rejected %dx%d texture (not a power of two)\n", xSize, ySize);
#endif
        lastError = PDDI_TEX_NOT_POW_2;
        return false;
    }

    int texSize;
#ifdef RAD_LINUX
    texSize = g_pglMaxTextureSize ? g_pglMaxTextureSize : 2048;
#else
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&texSize);
#endif
    
    if((xSize > texSize) || (ySize > texSize))
    {
        lastError = PDDI_TEX_TOO_BIG;
        return false;
    }

    bits = new char*[nMipMap+1];
    for(int i = 0; i < nMipMap+1; i++)
        bits[i] = new char[(xSize>>i)*(ySize>>i)*4];

    lock.depth = 32;
    lock.format = PDDI_PIXEL_ARGB8888;

    if(context->GetDisplay()->ExtBGRA())
    {
        lock.native = true;
        lock.rgbaLShift[0] = lock.rgbaRShift[0] =
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaLShift[2] = lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x00ff0000;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x000000ff;
        lock.rgbaMask[3] = 0xff000000;
    }
    else
    {
        lock.native = false;
        lock.rgbaRShift[0] = 16;
        lock.rgbaLShift[2] = 16;

        lock.rgbaLShift[0] = 
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x000000ff;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x00ff0000;
        lock.rgbaMask[3] = 0xff000000;
    }

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, (float)((xSize * ySize * 4) / 1024));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, 1);

    return true;
}

pglTexture::pglTexture(pglContext* c)
{
    context = c;
    contextID = c->contextID;
    bits = NULL;
    gltexture = 0;
    priority = 15;
}

pglTexture::~pglTexture()
{
    for(int i = 0; i < nMipMap+1; i++)
        delete bits[i];

    if(bits) delete [] bits;

#ifdef RAD_LINUX
    // Linux port: the GL copy was never freed, so every texture unloaded (e.g. on a level change) kept its video
    // memory until exit. A gltexture from before a display reset belongs to no one (SetGLState dropped it).
    if(contextID == context->contextID)
        pglDeleteTexture(gltexture);
#endif

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, -(float)((xSize * ySize * 4) / 1024));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, -1);
}

pddiPixelFormat pglTexture::GetPixelFormat()
{
    return PDDI_PIXEL_ARGB8888;
}

int   pglTexture::GetWidth()
{
    return xSize;
}

int   pglTexture::GetHeight()
{
    return ySize;
}

int   pglTexture::GetDepth()
{
    return 32;
}

int   pglTexture::GetNumMipMaps()
{
    return nMipMap;
}

int pglTexture::GetAlphaDepth()
{
    return 8;
}

pddiLockInfo* pglTexture::Lock(int mipMap, pddiRect* rect)
{
    PDDIASSERT(mipMap <= nMipMap);

    lock.width = 1 << (log2X-mipMap);
    lock.height = 1 << (log2Y-mipMap);
    lock.pitch = -lock.width * 4;
    lock.bits = bits[mipMap] + (lock.width * (lock.height-1) * 4);

    return &lock;
}

void pglTexture::Unlock(int mipLevel)
{
#ifdef RAD_LINUX
    pglDeleteTexture(gltexture);        // re-uploaded from bits[] by SetGLState on the render thread
#else
    glDeleteTextures(1,&gltexture);
#endif
    gltexture = 0;
}

void pglTexture::SetPriority(int p)
{
    priority = p;
#ifndef RAD_LINUX
    float fpriority = float(p) / 31.0f;
    glPrioritizeTextures(1,&gltexture,&fpriority);  // Linux port: not in core OpenGL (drivers ignored it anyway)
#endif
}

int pglTexture::GetPriority(void)
{
    return priority;
}

// paging control
void pglTexture::Prefetch(void)
{
}

void pglTexture::Discard(void)
{
}

// palette managment
int pglTexture::GetNumPaletteEntries(void)
{
    return 0;
}

void pglTexture::SetPalette(int nEntries, pddiColour* palette)
{
}

int pglTexture::GetPalette(pddiColour* palette)
{
    return 0;
}



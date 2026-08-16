//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#ifndef _GLTEX_HPP_
#define _GLTEX_HPP_
                 
#include <pddi/pddi.hpp>
#include <pddi/pdditype.hpp>
class pglContext;
         
class pglTexture : public pddiTexture
{
public:
    pglTexture(pglContext*);
    ~pglTexture();

    bool Create(int xSize, int ySize, int bpp, int alphaDepth, int nMip, pddiTextureType type = PDDI_TEXTYPE_RGB, pddiTextureUsageHint usageHint = PDDI_USAGE_STATIC);

    pddiPixelFormat GetPixelFormat();
    int GetWidth();
    int GetHeight();
    int GetDepth();
    int GetNumMipMaps();
    int GetAlphaDepth();

    // palette managment
    int GetNumPaletteEntries(void);
    void SetPalette(int nEntries, pddiColour* palette);
    int GetPalette(pddiColour* palette);

    // texture upload
    pddiLockInfo* Lock(int mipLevel, pddiRect* rect = 0);
    void Unlock(int mipLevel);

    // paging control
    void Prefetch(void);
    void Discard(void);
    void SetPriority(int priority);
    int GetPriority();

    // gl specific stuff
    void SetGLState(void);

protected:
    pglContext* context;
    unsigned contextID;

    int log2X, log2Y;
    int xSize, ySize;
    pddiTextureType type;
    int nMipMap;

    unsigned int gltexture;
    int priority;

    pddiLockInfo lock;

    char** bits;

#ifdef RAD_LINUX
    // Linux port: DXT textures keep their blocks (as D3D8 did); 0 for 32-bit ARGB
    unsigned compressedFormat;
    int blockBytes;
    int MipBytes(int mip);
    void UploadCompressed(void);
    bool hasMipmaps;            // generated after upload (uncompressed textures)

public:
    // the filter, address mode and (when the texture-filtering settings allow) mipmapping and anisotropy for this
    // texture as bound on the active unit; GL tables in glmat.cpp
    void SetSampling(int filterMode, unsigned wrap, unsigned magFilter, unsigned minFilter);
#endif
};
#endif


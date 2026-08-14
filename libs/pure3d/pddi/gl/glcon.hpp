//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#ifndef _GLCON_HPP_
#define _GLCON_HPP_

#define BUFFERED_VERTS 1024

#include <pddi/pddi.hpp>
#include <pddi/base/basecontext.hpp>

class pglDisplay;
class pglDevice;
class pglPipeline;

class pglExtContext;
class pglExtGamma;

//--------------------------------------------------------------
class pglContext : public pddiBaseContext
{
public :
    pglContext(pglDevice* dev, pglDisplay* disp);
    ~pglContext();

    // frame synchronisation
    void BeginFrame();
    void EndFrame();

    // buffer clearing
    void Clear(unsigned bufferMask);

    // viewport clipping
    void SetScissor(pddiRect* rect);

    // immediate mode prim rendering
    pddiPrimStream* BeginPrims(pddiShader* material, pddiPrimType primType, unsigned vertexType, int vertexCount = 0, unsigned pass = 0);
    void EndPrims(pddiPrimStream* stream);

    // retained mode prim rendering
    void DrawPrimBuffer(pddiShader* material, pddiPrimBuffer* buffer);

    // lighting
    int GetMaxLights();
    void SetAmbientLight(pddiColour col);

    // backface culling
    void SetCullMode(pddiCullMode mode);
    // Linux port: a material's culling, PGL_CULL_* (glpipeline.hpp): two-sided materials (and custom shaders with
    // #pragma shar_cull none) turn it off until a one-sided one is set, as in the D3D8 pddi
    void SetMaterialCull(int cull);

    // z-buffer control
    void EnableZBuffer(bool enable);
    void SetZCompare(pddiCompareMode compareMode);
    void SetZWrite(bool);
    void SetColourWrite(bool red, bool green, bool blue, bool alpha);   // Linux port: was not implemented
    void SetZBias(float bias);
    void SetZRange(float n, float f);

    // stencil buffer control
    void EnableStencilBuffer(bool enable);
    void SetStencilCompare(pddiCompareMode compare);
    void SetStencilRef(int ref);
    void SetStencilMask(unsigned mask);
    void SetStencilWriteMask(unsigned mask);
    void SetStencilOp(pddiStencilOp failOp, pddiStencilOp zFailOp, pddiStencilOp zPassOp);

    // polygon fill
    void SetFillMode(pddiFillMode mode);

    // fog
    void EnableFog(bool enable);
    void SetFog(pddiColour colour, float start, float end);

    // utility
    int GetMaxTextureDimension(void);

    // extensions
    pddiExtension* GetExtension(unsigned extID);
    bool VerifyExtension(unsigned extID);

    // internal pddiglfunctions
    pglDisplay* GetDisplay(void) {return display;}

    unsigned contextID;

protected:
    void LoadHardwareMatrix(pddiMatrixType id);
    void SetupHardwareProjection(void);
    void SetupHardwareLight(int);
    void  BeginTiming(void);
    float EndTiming(void);

    void SetVertexArray(unsigned descr, void* data, int count);

    pglDevice* device;
    pglDisplay* display;
    pglPipeline* pipeline;      // Linux port: the display's (OpenGL 3.3 core: glpipeline.hpp)
    int materialCull;
    void ApplyCull(void);

    pglExtContext* extContext;
    pglExtGamma* extGamma;

    pddiShader* defaultShader;

//   int nBuffered;
//   unsigned currentMatId;
//   pddiScreenVertex buffer[BUFFERED_VERTS];

// void FlushBuffer(void);
// BOOL ZTestQuery(float x, float y, float z);
// void ZTestPoint(float x, float y, float z);
//   pddiFillMode fillMode;

//   int yRes;

//   ULONG compareMode;
//   BOOL zWrite;
//   float alphaRef;
};

class pglPrimBufferStream;

class pglPrimBuffer : public pddiPrimBuffer
{
public:
    pglPrimBuffer(pglContext* context, pddiPrimType type, unsigned vertexFormat, int nVertex, int nIndex);
    ~pglPrimBuffer();

    pddiPrimBufferStream* Lock();
    void Unlock(pddiPrimBufferStream* stream);

    unsigned char* LockIndexBuffer();
    void UnlockIndexBuffer(int count);

    void SetIndices(unsigned short* indices, int count);

    bool CheckMemImageVersion(int version) { return false; }
    void* LockMemImage(unsigned) { return NULL;}
    void UnlockMemImage() { }
    unsigned GetMemImageLength() {return 0; }
    void SetMemImageParam(unsigned param, unsigned value) { /**/ }

    // Linux port: draw only the first size indices (or vertices), as pddidx8 does
    void SetUsedSize(int size) { useSize = size > 0 ? (unsigned) size : 0; }

    void Display(void);

protected:
    void Upload(void);

    friend class pglPrimBufferStream;
    pglPrimBufferStream* stream;
    pglContext* context;

    pddiPrimType primType;
    unsigned vertexType;

    int nStrips;
    int* strips;

    float* coord;
    float* normal;
    float* uv;
    unsigned char* colour;

    unsigned allocated;
    unsigned total;

    unsigned short* indices;
    unsigned indexCount;

    // Linux port: as pddidx8, draw the indices last set (SetIndices), not all allocated ones (the rest were
    // uninitialised: GL then read vertices from anywhere); the arrays' own starts, for Unlock and the stream's
    // bounds check
    unsigned indexUsed;
    unsigned useSize;
    float* coordBase;
    float* normalBase;
    float* uvBase;
    unsigned char* colourBase;

    // Linux port: GPU copies of the arrays (all attributes in one buffer, at these offsets) and the indices,
    // uploaded by Display when changed
    unsigned vertexBuffer;
    unsigned indexBuffer;
    unsigned normalOffset, uvOffset, colourOffset;
    bool verticesChanged;
    bool indicesChanged;
    unsigned uploads;

    float mem;
};
    
#endif


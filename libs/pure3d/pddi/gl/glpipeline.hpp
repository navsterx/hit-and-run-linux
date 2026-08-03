//=============================================================================
// Linux port: OpenGL 3.3 core renderer.
//=============================================================================

// The GL pddi was written for fixed-function OpenGL 1.x (built-in lights, materials, fog, matrix stacks, client-side
// vertex arrays, glBegin/glEnd). In a core profile none of that exists: pglPipeline keeps the same state on the CPU
// and draws through a GLSL program that computes what the fixed-function pipeline computed with the settings the pddi
// used (per-vertex lighting with separate specular, GL_MODULATE texturing, linear fog, alpha test). pglProgram is
// the general compile/link wrapper, for further shaders.

#ifndef _GLPIPELINE_HPP_
#define _GLPIPELINE_HPP_

#include <pddi/gl/display_linux/gl.hpp>

class pglProgram
{
public:
    pglProgram();
    ~pglProgram();

    // compiles and links; on failure prints the driver's log (prefixed with name) and returns false
    bool Build(const char* name, const char* vertexSource, const char* fragmentSource);
    GLint Uniform(const char* name) const;
    GLuint Handle() const { return program; }

private:
    GLuint program;
};

// vertex attribute locations shared by every program
enum
{
    PGL_ATTRIB_POSITION = 0,
    PGL_ATTRIB_NORMAL = 1,
    PGL_ATTRIB_COLOUR = 2,
    PGL_ATTRIB_UV = 3
};

// one vertex of an immediate-mode stream (glBegin/glEnd replacement)
struct pglVertex
{
    float position[3];
    float normal[3];
    float uv[2];
    unsigned char colour[4];
};

const int pglMaxLights = 8;

class pglPipeline
{
public:
    // what pglMat::SetDevPass used to hand to glMaterial/glAlphaFunc/glEnable(GL_TEXTURE_2D)
    struct Material
    {
        bool textured;
        bool lit;
        int alphaCompare;           // pddiCompareMode, or -1 for no alpha test
        float alphaRef;
        float ambient[4];
        float diffuse[4];
        float specular[4];
        float emissive[4];
        float shininess;
    };

    pglPipeline();
    ~pglPipeline();

    // GL objects; render thread, with the context current. False if the shaders do not build.
    bool Init();

    // matrices (column-major, as glLoadMatrixf took them)
    void SetProjection(const float* m);
    void SetModelView(const float* m);
    void LoadIdentity();            // both, as BeginFrame did

    // lights: position (w = 0: direction towards the light) in the coordinates of the model-view loaded now, which
    // transforms it to eye space as glLightfv(GL_POSITION) did
    void SetLight(int i, bool enabled, const float* position, const float* colour, float attenA, float attenB, float attenC);
    void SetAmbient(const float* colour);

    void EnableFog(bool enable);
    void SetFog(const float* colour, float start, float end);

    void SetMaterial(const Material& m);

    // the "current" vertex values (glColor, glNormal, glTexCoord): used for attributes a vertex does not carry
    pglVertex current;

    // immediate mode: BeginStream, then set current values and Emit vertices, then EndStream draws them
    void BeginStream(GLenum primType);
    void Emit(float x, float y, float z);
    void EndStream();

    // before drawing anything: binds the program and vertex array object, uploads changed uniforms
    void Prepare();

    // after GL calls outside the pipeline that change the program or vertex array object (the gamma pass)
    void Unbind() { bound = false; }

    // for a draw without an array for attrib (prim buffers): use the current value instead
    void UseCurrent(GLuint attrib);

    // SHAR_GL_STATS=1: per-frame averages of draws, uniform and buffer uploads, printed every 5 s (from BeginFrame)
    void FrameStats();
    void FrameEnd();            // from EndFrame: GPU and CPU time of the frame's rendering
    struct Stats { unsigned frames, draws, streamDraws, matrixUploads, lightUploads, materialUploads, bufferUploads, bufferBytes;
                   double gpuMs, cpuMs; unsigned gpuFrames; } stats;

private:
    void UploadUniforms();

    bool ready;
    bool bound;
    pglProgram fixedProgram;
    GLuint vao;

    // streamed vertices: CPU staging, then a ring buffer on the GPU
    pglVertex* streamVerts;
    int streamCount, streamCapacity;
    GLenum streamPrim;
    GLuint streamBuffer;
    unsigned streamBufferSize, streamOffset;

    // state, and what changed since the last upload
    enum { DIRTY_MATRICES = 1, DIRTY_LIGHTS = 2, DIRTY_FOG = 4, DIRTY_MATERIAL = 8, DIRTY_ALL = 15 };
    unsigned dirty;

    float projection[16];
    float modelView[16];

    struct Light
    {
        bool enabled;
        float position[4];      // eye space
        float colour[3];
        float atten[3];
    } lights[pglMaxLights];
    float ambient[4];

    bool fogEnabled;
    float fogColour[3];
    float fogStart, fogEnd;

    Material material;

    // uniform locations
    GLint uProjection, uModelView, uNormalMatrix;
    GLint uLit, uMatAmbient, uMatDiffuse, uMatSpecular, uMatEmissive, uShininess;
    GLint uGlobalAmbient, uLightCount, uLightPosition, uLightColour, uLightAtten;
    GLint uTextured, uTexture, uFog, uFogColour, uFogEnd, uFogScale, uAlphaCompare, uAlphaRef;
};

// GL object deletion from any thread: deleted at once where a context is current, else queued for the render thread
void pglDeleteTexture(GLuint texture);
void pglDeleteBuffer(GLuint buffer);
void pglFlushPendingDeletes();      // render thread

#endif

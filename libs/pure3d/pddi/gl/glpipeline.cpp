//=============================================================================
// Linux port: OpenGL 3.3 core renderer (see glpipeline.hpp).
//=============================================================================

#include <pddi/gl/glpipeline.hpp>

#include <SDL2/SDL.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//-----------------------------------------------------------------------------
// pglProgram
//-----------------------------------------------------------------------------

pglProgram::pglProgram() : program(0)
{
}

pglProgram::~pglProgram()
{
    // the context is gone by the time the display is destroyed; the driver frees the program with it
}

static GLuint CompileShader(const char* name, GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[2048] = "";
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "PDDI: %s %s shader does not compile:\n%s\n", name,
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool pglProgram::Build(const char* name, const char* vertexSource, const char* fragmentSource)
{
    GLuint vs = CompileShader(name, GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileShader(name, GL_FRAGMENT_SHADER, fragmentSource);
    if(!vs || !fs)
    {
        if(vs) glDeleteShader(vs);
        if(fs) glDeleteShader(fs);
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, PGL_ATTRIB_POSITION, "aPosition");
    glBindAttribLocation(program, PGL_ATTRIB_NORMAL, "aNormal");
    glBindAttribLocation(program, PGL_ATTRIB_COLOUR, "aColour");
    glBindAttribLocation(program, PGL_ATTRIB_UV, "aUV");
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        char log[2048] = "";
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "PDDI: %s program does not link:\n%s\n", name, log);
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    return true;
}

GLint pglProgram::Uniform(const char* name) const
{
    return glGetUniformLocation(program, name);
}

//-----------------------------------------------------------------------------
// The fixed-function pipeline as the pddi configured it
//-----------------------------------------------------------------------------

// Lighting follows the OpenGL 1.x equations: per vertex, in eye space, an infinite viewer, light ambient terms 0
// (GL_AMBIENT was never set), separate specular (GL_EXT_separate_specular_color). Normals are transformed but not
// renormalised: GL_NORMALIZE was never enabled. Fog distance is |eye z|, as the drivers computed it.
static const char* s_fixedVertexSource =
    "#version 330 core\n"
    "in vec3 aPosition;\n"
    "in vec3 aNormal;\n"
    "in vec4 aColour;\n"
    "in vec2 aUV;\n"
    "uniform mat4 uProjection;\n"
    "uniform mat4 uModelView;\n"
    "uniform mat3 uNormalMatrix;\n"
    "uniform bool uLit;\n"
    "uniform vec4 uMatAmbient;\n"
    "uniform vec4 uMatDiffuse;\n"
    "uniform vec4 uMatSpecular;\n"
    "uniform vec4 uMatEmissive;\n"
    "uniform float uShininess;\n"
    "uniform vec4 uGlobalAmbient;\n"
    "uniform int uLightCount;\n"
    "uniform vec4 uLightPosition[8];\n"
    "uniform vec3 uLightColour[8];\n"
    "uniform vec3 uLightAtten[8];\n"
    "out vec4 vColour;\n"
    "out vec3 vSpecular;\n"
    "out vec2 vUV;\n"
    "out float vFogDistance;\n"
    "void main()\n"
    "{\n"
    "    vec4 eye = uModelView * vec4(aPosition, 1.0);\n"
    "    gl_Position = uProjection * eye;\n"
    "    vUV = aUV;\n"
    "    vFogDistance = abs(eye.z);\n"
    "    if(!uLit)\n"
    "    {\n"
    "        vColour = aColour;\n"
    "        vSpecular = vec3(0.0);\n"
    "        return;\n"
    "    }\n"
    "    vec3 n = uNormalMatrix * aNormal;\n"
    "    vec3 v = eye.xyz / eye.w;\n"
    "    vec3 colour = uMatEmissive.rgb + uMatAmbient.rgb * uGlobalAmbient.rgb;\n"
    "    vec3 specular = vec3(0.0);\n"
    "    for(int i = 0; i < uLightCount; i++)\n"
    "    {\n"
    "        vec3 l;\n"
    "        float attenuation = 1.0;\n"
    "        if(uLightPosition[i].w == 0.0)\n"
    "        {\n"
    "            l = normalize(uLightPosition[i].xyz);\n"
    "        }\n"
    "        else\n"
    "        {\n"
    "            vec3 d = uLightPosition[i].xyz / uLightPosition[i].w - v;\n"
    "            float distance = length(d);\n"
    "            l = d / distance;\n"
    "            attenuation = 1.0 / (uLightAtten[i].x + uLightAtten[i].y * distance + uLightAtten[i].z * distance * distance);\n"
    "        }\n"
    "        float nl = dot(n, l);\n"
    "        colour += attenuation * max(nl, 0.0) * uMatDiffuse.rgb * uLightColour[i];\n"
    "        if(nl > 0.0)\n"
    "        {\n"
    "            float nh = max(dot(n, normalize(l + vec3(0.0, 0.0, 1.0))), 0.0);\n"
    "            float s = uShininess > 0.0 ? pow(nh, uShininess) : 1.0;\n"
    "            specular += attenuation * s * uMatSpecular.rgb * uLightColour[i];\n"
    "        }\n"
    "    }\n"
    "    vColour = clamp(vec4(colour, uMatDiffuse.a), 0.0, 1.0);\n"
    "    vSpecular = clamp(specular, 0.0, 1.0);\n"
    "}\n";

// GL_MODULATE texturing, colour sum, linear fog (RGB only), then the alpha test (pddiCompareMode order)
static const char* s_fixedFragmentSource =
    "#version 330 core\n"
    "in vec4 vColour;\n"
    "in vec3 vSpecular;\n"
    "in vec2 vUV;\n"
    "in float vFogDistance;\n"
    "uniform bool uTextured;\n"
    "uniform sampler2D uTexture;\n"
    "uniform bool uFog;\n"
    "uniform vec3 uFogColour;\n"
    "uniform float uFogEnd;\n"
    "uniform float uFogScale;\n"
    "uniform int uAlphaCompare;\n"
    "uniform float uAlphaRef;\n"
    "out vec4 fragColour;\n"
    "void main()\n"
    "{\n"
    "    vec4 c = vColour;\n"
    "    if(uTextured)\n"
    "        c *= texture(uTexture, vUV);\n"
    "    c.rgb = min(c.rgb + vSpecular, 1.0);\n"
    "    if(uFog)\n"
    "        c.rgb = mix(uFogColour, c.rgb, clamp((uFogEnd - vFogDistance) * uFogScale, 0.0, 1.0));\n"
    "    bool pass = true;\n"
    "    if(uAlphaCompare == 0) pass = false;\n"
    "    else if(uAlphaCompare == 2) pass = c.a < uAlphaRef;\n"
    "    else if(uAlphaCompare == 3) pass = c.a <= uAlphaRef;\n"
    "    else if(uAlphaCompare == 4) pass = c.a > uAlphaRef;\n"
    "    else if(uAlphaCompare == 5) pass = c.a >= uAlphaRef;\n"
    "    else if(uAlphaCompare == 6) pass = c.a == uAlphaRef;\n"
    "    else if(uAlphaCompare == 7) pass = c.a != uAlphaRef;\n"
    "    if(!pass)\n"
    "        discard;\n"
    "    fragColour = c;\n"
    "}\n";

static void Identity(float* m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void SetColour(float* d, const float* s, int n)
{
    for(int i = 0; i < n; i++)
        d[i] = s[i];
}

pglPipeline::pglPipeline()
{
    ready = false;
    bound = false;
    vao = 0;
    streamVerts = NULL;
    streamCount = streamCapacity = 0;
    streamPrim = GL_TRIANGLES;
    streamBuffer = 0;
    streamBufferSize = streamOffset = 0;
    dirty = DIRTY_ALL;

    // the initial GL state
    Identity(projection);
    Identity(modelView);
    memset(lights, 0, sizeof(lights));
    for(int i = 0; i < pglMaxLights; i++)
        lights[i].atten[0] = 1.0f;
    ambient[0] = ambient[1] = ambient[2] = 0.2f;
    ambient[3] = 1.0f;
    fogEnabled = false;
    fogColour[0] = fogColour[1] = fogColour[2] = 0.0f;
    fogStart = 0.0f;
    fogEnd = 1.0f;

    memset(&material, 0, sizeof(material));
    material.alphaCompare = -1;

    memset(&stats, 0, sizeof(stats));
    memset(&current, 0, sizeof(current));
    current.normal[2] = 1.0f;
    current.colour[0] = current.colour[1] = current.colour[2] = current.colour[3] = 255;
}

pglPipeline::~pglPipeline()
{
    free(streamVerts);
}

bool pglPipeline::Init()
{
    if(ready)
        return true;

    if(!fixedProgram.Build("fixed-function", s_fixedVertexSource, s_fixedFragmentSource))
        return false;

    GLuint p = fixedProgram.Handle();
    glUseProgram(p);
    uProjection = fixedProgram.Uniform("uProjection");
    uModelView = fixedProgram.Uniform("uModelView");
    uNormalMatrix = fixedProgram.Uniform("uNormalMatrix");
    uLit = fixedProgram.Uniform("uLit");
    uMatAmbient = fixedProgram.Uniform("uMatAmbient");
    uMatDiffuse = fixedProgram.Uniform("uMatDiffuse");
    uMatSpecular = fixedProgram.Uniform("uMatSpecular");
    uMatEmissive = fixedProgram.Uniform("uMatEmissive");
    uShininess = fixedProgram.Uniform("uShininess");
    uGlobalAmbient = fixedProgram.Uniform("uGlobalAmbient");
    uLightCount = fixedProgram.Uniform("uLightCount");
    uLightPosition = fixedProgram.Uniform("uLightPosition");
    uLightColour = fixedProgram.Uniform("uLightColour");
    uLightAtten = fixedProgram.Uniform("uLightAtten");
    uTextured = fixedProgram.Uniform("uTextured");
    uTexture = fixedProgram.Uniform("uTexture");
    uFog = fixedProgram.Uniform("uFog");
    uFogColour = fixedProgram.Uniform("uFogColour");
    uFogEnd = fixedProgram.Uniform("uFogEnd");
    uFogScale = fixedProgram.Uniform("uFogScale");
    uAlphaCompare = fixedProgram.Uniform("uAlphaCompare");
    uAlphaRef = fixedProgram.Uniform("uAlphaRef");
    glUniform1i(uTexture, 0);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(PGL_ATTRIB_POSITION);     // every draw has positions

    glGenBuffers(1, &streamBuffer);
    streamBufferSize = 4 * 1024 * 1024;
    glBindBuffer(GL_ARRAY_BUFFER, streamBuffer);
    glBufferData(GL_ARRAY_BUFFER, streamBufferSize, NULL, GL_STREAM_DRAW);
    streamOffset = 0;

    dirty = DIRTY_ALL;
    ready = true;
    return true;
}

// setters skip values that did not change: the game sets the same material and matrices over and over
void pglPipeline::SetProjection(const float* m)
{
    if(memcmp(projection, m, sizeof(projection)) == 0)
        return;
    memcpy(projection, m, sizeof(projection));
    dirty |= DIRTY_MATRICES;
}

void pglPipeline::SetModelView(const float* m)
{
    if(memcmp(modelView, m, sizeof(modelView)) == 0)
        return;
    memcpy(modelView, m, sizeof(modelView));
    dirty |= DIRTY_MATRICES;
}

void pglPipeline::LoadIdentity()
{
    Identity(projection);
    Identity(modelView);
    dirty |= DIRTY_MATRICES;
}

void pglPipeline::SetLight(int i, bool enabled, const float* p, const float* colour, float attenA, float attenB, float attenC)
{
    if(i < 0 || i >= pglMaxLights)
        return;
    Light& l = lights[i];
    l.enabled = enabled;
    for(int r = 0; r < 4; r++)
        l.position[r] = modelView[r] * p[0] + modelView[4 + r] * p[1] + modelView[8 + r] * p[2] + modelView[12 + r] * p[3];
    SetColour(l.colour, colour, 3);
    l.atten[0] = attenA;
    l.atten[1] = attenB;
    l.atten[2] = attenC;
    dirty |= DIRTY_LIGHTS;
}

void pglPipeline::SetAmbient(const float* colour)
{
    SetColour(ambient, colour, 4);
    dirty |= DIRTY_LIGHTS;
}

void pglPipeline::EnableFog(bool enable)
{
    fogEnabled = enable;
    dirty |= DIRTY_FOG;
}

void pglPipeline::SetFog(const float* colour, float start, float end)
{
    SetColour(fogColour, colour, 3);
    fogStart = start;
    fogEnd = end;
    dirty |= DIRTY_FOG;
}

void pglPipeline::SetMaterial(const Material& m)      // m fully initialised (memset), padding included
{
    if(memcmp(&material, &m, sizeof(material)) == 0)
        return;
    material = m;
    dirty |= DIRTY_MATERIAL;
}

void pglPipeline::UploadUniforms()
{
    stats.matrixUploads += (dirty & DIRTY_MATRICES) != 0;
    stats.lightUploads += (dirty & DIRTY_LIGHTS) != 0;
    stats.materialUploads += (dirty & DIRTY_MATERIAL) != 0;

    if(dirty & DIRTY_MATRICES)
    {
        glUniformMatrix4fv(uProjection, 1, GL_FALSE, projection);
        glUniformMatrix4fv(uModelView, 1, GL_FALSE, modelView);

        // normals go through the inverse transpose of the model-view's upper 3x3: its columns are the cross
        // products of the matrix's columns, over the determinant
        const float* a0 = &modelView[0];
        const float* a1 = &modelView[4];
        const float* a2 = &modelView[8];
        float n[9];
        n[0] = a1[1] * a2[2] - a1[2] * a2[1];
        n[1] = a1[2] * a2[0] - a1[0] * a2[2];
        n[2] = a1[0] * a2[1] - a1[1] * a2[0];
        n[3] = a2[1] * a0[2] - a2[2] * a0[1];
        n[4] = a2[2] * a0[0] - a2[0] * a0[2];
        n[5] = a2[0] * a0[1] - a2[1] * a0[0];
        n[6] = a0[1] * a1[2] - a0[2] * a1[1];
        n[7] = a0[2] * a1[0] - a0[0] * a1[2];
        n[8] = a0[0] * a1[1] - a0[1] * a1[0];
        float det = a0[0] * n[0] + a0[1] * n[1] + a0[2] * n[2];
        if(det != 0.0f)
        {
            for(int i = 0; i < 9; i++)
                n[i] /= det;
        }
        glUniformMatrix3fv(uNormalMatrix, 1, GL_FALSE, n);
    }

    if(dirty & DIRTY_LIGHTS)
    {
        float position[pglMaxLights][4], colour[pglMaxLights][3], atten[pglMaxLights][3];
        int count = 0;
        for(int i = 0; i < pglMaxLights; i++)
        {
            if(!lights[i].enabled)
                continue;
            memcpy(position[count], lights[i].position, sizeof(position[0]));
            memcpy(colour[count], lights[i].colour, sizeof(colour[0]));
            memcpy(atten[count], lights[i].atten, sizeof(atten[0]));
            count++;
        }
        glUniform1i(uLightCount, count);
        if(count)
        {
            glUniform4fv(uLightPosition, count, &position[0][0]);
            glUniform3fv(uLightColour, count, &colour[0][0]);
            glUniform3fv(uLightAtten, count, &atten[0][0]);
        }
        glUniform4fv(uGlobalAmbient, 1, ambient);
    }

    if(dirty & DIRTY_FOG)
    {
        glUniform1i(uFog, fogEnabled);
        glUniform3fv(uFogColour, 1, fogColour);
        glUniform1f(uFogEnd, fogEnd);
        glUniform1f(uFogScale, fogEnd != fogStart ? 1.0f / (fogEnd - fogStart) : 1.0f);
    }

    if(dirty & DIRTY_MATERIAL)
    {
        glUniform1i(uTextured, material.textured);
        glUniform1i(uLit, material.lit);
        glUniform4fv(uMatAmbient, 1, material.ambient);
        glUniform4fv(uMatDiffuse, 1, material.diffuse);
        glUniform4fv(uMatSpecular, 1, material.specular);
        glUniform4fv(uMatEmissive, 1, material.emissive);
        glUniform1f(uShininess, material.shininess);
        glUniform1i(uAlphaCompare, material.alphaCompare < 0 ? 1 : material.alphaCompare);    // 1: always
        glUniform1f(uAlphaRef, material.alphaRef);
    }

    dirty = 0;
}

void pglPipeline::Prepare()
{
    if(!bound)
    {
        glUseProgram(fixedProgram.Handle());
        glBindVertexArray(vao);
        bound = true;
    }
    if(dirty)
        UploadUniforms();
    stats.draws++;
}

static int s_statsEnabled = -1;
static GLuint s_timeQuery[2];
static int s_timeQueryIndex = 0;
static bool s_timeQueryUsed[2];
static Uint64 s_frameStart;

void pglPipeline::FrameStats()
{
    static Uint32 last = 0;
    if(s_statsEnabled < 0)
    {
        const char* e = getenv("SHAR_GL_STATS");
        s_statsEnabled = e && e[0] == '1';
        last = SDL_GetTicks();
        if(s_statsEnabled)
            glGenQueries(2, s_timeQuery);
    }
    if(!s_statsEnabled)
        return;

    // the other query is a frame old: its result is (nearly always) in
    int other = s_timeQueryIndex ^ 1;
    if(s_timeQueryUsed[other])
    {
        GLuint64 ns = 0;
        glGetQueryObjectui64v(s_timeQuery[other], GL_QUERY_RESULT, &ns);
        stats.gpuMs += ns / 1e6;
        stats.gpuFrames++;
        s_timeQueryUsed[other] = false;
    }
    glBeginQuery(GL_TIME_ELAPSED, s_timeQuery[s_timeQueryIndex]);
    s_frameStart = SDL_GetPerformanceCounter();

    stats.frames++;
    if(SDL_GetTicks() - last < 5000)
        return;
    last = SDL_GetTicks();
    float f = (float)stats.frames;
    fprintf(stderr, "[gl] per frame: %.0f draws (%.0f streamed), uniforms: %.0f matrix %.0f light %.0f material, "
            "%.1f buffer uploads (%.0f KB); render time %.3f ms CPU, %.3f ms GPU\n", stats.draws / f, stats.streamDraws / f,
            stats.matrixUploads / f, stats.lightUploads / f, stats.materialUploads / f, stats.bufferUploads / f,
            stats.bufferBytes / f / 1024.0f, stats.cpuMs / f, stats.gpuFrames ? stats.gpuMs / stats.gpuFrames : 0.0);
    memset(&stats, 0, sizeof(stats));
}

void pglPipeline::FrameEnd()
{
    if(s_statsEnabled <= 0)
        return;
    glEndQuery(GL_TIME_ELAPSED);
    s_timeQueryUsed[s_timeQueryIndex] = true;
    s_timeQueryIndex ^= 1;
    stats.cpuMs += (SDL_GetPerformanceCounter() - s_frameStart) * 1000.0 / SDL_GetPerformanceFrequency();
}

void pglPipeline::UseCurrent(GLuint attrib)
{
    glDisableVertexAttribArray(attrib);
    switch(attrib)
    {
        case PGL_ATTRIB_NORMAL:
            glVertexAttrib3f(attrib, current.normal[0], current.normal[1], current.normal[2]);
            break;
        case PGL_ATTRIB_COLOUR:
            glVertexAttrib4f(attrib, current.colour[0] / 255.0f, current.colour[1] / 255.0f,
                             current.colour[2] / 255.0f, current.colour[3] / 255.0f);
            break;
        case PGL_ATTRIB_UV:
            glVertexAttrib2f(attrib, current.uv[0], current.uv[1]);
            break;
    }
}

void pglPipeline::BeginStream(GLenum primType)
{
    streamPrim = primType;
    streamCount = 0;
}

void pglPipeline::Emit(float x, float y, float z)
{
    if(streamCount == streamCapacity)
    {
        int capacity = streamCapacity ? streamCapacity * 2 : 1024;
        pglVertex* v = (pglVertex*)realloc(streamVerts, capacity * sizeof(pglVertex));
        if(!v)
            return;
        streamVerts = v;
        streamCapacity = capacity;
    }
    pglVertex& v = streamVerts[streamCount++];
    v = current;
    v.position[0] = x;
    v.position[1] = y;
    v.position[2] = z;
}

void pglPipeline::EndStream()
{
    if(!ready || streamCount == 0)
        return;

    Prepare();

    // append to the ring buffer without waiting for draws still reading it; when full, orphan it (the driver hands
    // out fresh storage) and start again at the beginning
    unsigned size = streamCount * sizeof(pglVertex);
    glBindBuffer(GL_ARRAY_BUFFER, streamBuffer);
    if(size > streamBufferSize)
    {
        while(streamBufferSize < size)
            streamBufferSize *= 2;
        glBufferData(GL_ARRAY_BUFFER, streamBufferSize, NULL, GL_STREAM_DRAW);
        streamOffset = 0;
    }
    else if(streamOffset + size > streamBufferSize)
    {
        glBufferData(GL_ARRAY_BUFFER, streamBufferSize, NULL, GL_STREAM_DRAW);
        streamOffset = 0;
    }
    glBufferSubData(GL_ARRAY_BUFFER, streamOffset, size, streamVerts);

    GLsizei stride = sizeof(pglVertex);
    const char* base = (const char*)(size_t)streamOffset;
    glVertexAttribPointer(PGL_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, stride, base + offsetof(pglVertex, position));
    glVertexAttribPointer(PGL_ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, stride, base + offsetof(pglVertex, normal));
    glVertexAttribPointer(PGL_ATTRIB_COLOUR, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, base + offsetof(pglVertex, colour));
    glVertexAttribPointer(PGL_ATTRIB_UV, 2, GL_FLOAT, GL_FALSE, stride, base + offsetof(pglVertex, uv));
    glEnableVertexAttribArray(PGL_ATTRIB_NORMAL);
    glEnableVertexAttribArray(PGL_ATTRIB_COLOUR);
    glEnableVertexAttribArray(PGL_ATTRIB_UV);

    glDrawArrays(streamPrim, 0, streamCount);
    stats.streamDraws++;
    streamOffset += (size + 63) & ~63u;
}

//-----------------------------------------------------------------------------
// Deferred deletion
//-----------------------------------------------------------------------------

// Pure3D creates and frees textures and prim buffers on its loader threads. Those threads have shared contexts (see
// gldisplay.cpp) and delete at once; a thread without one queues the name for the render thread.
static pthread_mutex_t s_PendingDeleteLock = PTHREAD_MUTEX_INITIALIZER;
static GLuint s_PendingTextures[1024];
static int s_PendingTextureCount = 0;
static GLuint s_PendingBuffers[1024];
static int s_PendingBufferCount = 0;

static bool HaveGLContext()
{
    return SDL_GL_GetCurrentContext() != NULL;
}

void pglDeleteTexture(GLuint texture)
{
    if(!texture)
        return;
    if(HaveGLContext())
    {
        glDeleteTextures(1, &texture);
        return;
    }
    pthread_mutex_lock(&s_PendingDeleteLock);
    if(s_PendingTextureCount < 1024)
        s_PendingTextures[s_PendingTextureCount++] = texture;
    pthread_mutex_unlock(&s_PendingDeleteLock);
}

void pglDeleteBuffer(GLuint buffer)
{
    if(!buffer)
        return;
    if(HaveGLContext())
    {
        glDeleteBuffers(1, &buffer);
        return;
    }
    pthread_mutex_lock(&s_PendingDeleteLock);
    if(s_PendingBufferCount < 1024)
        s_PendingBuffers[s_PendingBufferCount++] = buffer;
    pthread_mutex_unlock(&s_PendingDeleteLock);
}

void pglFlushPendingDeletes()
{
    pthread_mutex_lock(&s_PendingDeleteLock);
    if(s_PendingTextureCount)
    {
        glDeleteTextures(s_PendingTextureCount, s_PendingTextures);
        s_PendingTextureCount = 0;
    }
    if(s_PendingBufferCount)
    {
        glDeleteBuffers(s_PendingBufferCount, s_PendingBuffers);
        s_PendingBufferCount = 0;
    }
    pthread_mutex_unlock(&s_PendingDeleteLock);
}

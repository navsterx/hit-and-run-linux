//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#if defined(RAD_WIN32) && !defined(RAD_LINUX)
    #include <pddi/gl/display_win32/gldisplay.hpp>
    #include <pddi/gl/display_win32/gl.hpp>
#endif

#ifdef RAD_LINUX
    #include <pddi/gl/display_linux/gldisplay.hpp>
    #include <pddi/gl/display_linux/gl.hpp>
#endif


#include <pddi/gl/glcon.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <assert.h>

#include <unistd.h>
#include <sys/time.h>

bool pglDisplay::CheckExtension( char *extName )
{
    char *p = (char *) glGetString(GL_EXTENSIONS);

    if((p == NULL) || (extName == NULL))
        return false;
    
    char *end;
    int extNameLen;

    extNameLen = strlen(extName);
    end = p + strlen(p);

    while (p < end) {
        int n = strcspn(p, " ");
        if ((extNameLen == n) && (strncmp(extName, p, n) == 0)) {
             return true;
        }
        p += (n + 1);
    }
    return false;
}

pglDisplay::pglDisplay()
{
    mode = PDDI_DISPLAY_WINDOW;
    winWidth = 640;
    winHeight = 480;
    winBitDepth = 32;

    context = NULL;

    window = NULL;
    glContext = NULL;

    extBGRA = false;

    gammaR = gammaG = gammaB = 1.0f;

    reset = true;
	m_ForceVSync = false;
}

pglDisplay::~pglDisplay()
{
}

void pglDisplay::SetWindowHandle(void* sdlWindow)
{
    window = (SDL_Window*)sdlWindow;
}

void* pglDisplay::GetWindowHandle()
{
    return (void*)window;
}

// Direct3D 8 let every thread use the device; Pure3D loads (and sets some state) on worker threads.
// Worker threads get one of these shared GL contexts at thread start (shar_SetThreadStartHook), so their
// GL calls are valid: textures are shared with the render context, other state stays local to them.
#include <windows.h>
#include <pthread.h>
static const int kWorkerContexts = 6;
static SDL_Window*   s_WorkerWindow = NULL;
static SDL_GLContext s_WorkerContexts[ kWorkerContexts ];
static int           s_WorkerContextCount = 0;
static pthread_mutex_t s_WorkerLock = PTHREAD_MUTEX_INITIALIZER;

static void AttachWorkerContext( void )
{
    if( SDL_GL_GetCurrentContext( ) != NULL ) return;
    pthread_mutex_lock( &s_WorkerLock );
    SDL_GLContext ctx = s_WorkerContextCount > 0 ? s_WorkerContexts[ --s_WorkerContextCount ] : NULL;
    pthread_mutex_unlock( &s_WorkerLock );
    if( ctx != NULL )
        SDL_GL_MakeCurrent( s_WorkerWindow, ctx );
}

static void CreateWorkerContexts( SDL_Window* window, SDL_GLContext mainContext )
{
    if( s_WorkerWindow != NULL ) return;
    s_WorkerWindow = window;
    SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1 );
    for( int i = 0; i < kWorkerContexts; i++ )
    {
        SDL_GLContext ctx = SDL_GL_CreateContext( window );     // shares with the current (main) context
        if( ctx == NULL ) break;
        s_WorkerContexts[ s_WorkerContextCount++ ] = ctx;
        SDL_GL_MakeCurrent( window, mainContext );
    }
    SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0 );
    SDL_GL_MakeCurrent( window, mainContext );
    shar_SetThreadStartHook( AttachWorkerContext );
}

// SDL2 replaces the original glX code: the platform layer creates the SDL window (with SDL_WINDOW_OPENGL
// and the GL attributes already set); here the GL context is created on it and made current.
bool pglDisplay::InitDisplay(const pddiDisplayInit* init)
{
    assert(window && "pglDisplay: SetWindowHandle(SDL_Window*) must be called before InitDisplay");
    mode = init->displayMode;
    winBitDepth = init->bpp;

    // Borderless fullscreen covers the desktop at its own size (no display mode change). Exclusive fullscreen
    // switches the display to the mode closest to the requested size (rendering below the native resolution,
    // e.g. for speed). In a window the requested size is the window size.
    if(mode == PDDI_DISPLAY_FULLSCREEN_DESKTOP)
    {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else if(mode == PDDI_DISPLAY_FULLSCREEN)
    {
        // leave fullscreen first: switching straight from borderless (or between exclusive modes) kept the old
        // size under XWayland
        SDL_SetWindowFullscreen(window, 0);
        SDL_DisplayMode want, closest;
        memset(&want, 0, sizeof(want));
        want.w = init->xsize;
        want.h = init->ysize;
        int display = SDL_GetWindowDisplayIndex(window);
        if(SDL_GetClosestDisplayMode(display < 0 ? 0 : display, &want, &closest) != NULL)
        {
            SDL_SetWindowDisplayMode(window, &closest);
        }
        SDL_SetWindowSize(window, init->xsize, init->ysize);
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    }
    else
    {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, init->xsize, init->ysize);
    }

    if(!glContext)
    {
        glContext = SDL_GL_CreateContext(window);
        if(!glContext)
        {
            fprintf(stderr, "PDDI: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
            return false;
        }
    }
    SDL_GL_MakeCurrent(window, glContext);
    // Linux port: always sync to the display (as the D3D8 build effectively did). The game's physics and AI
    // were tuned for 30-60 fps; unsynced it ran at thousands of fps with 0 ms frames (NaN traffic, unstable cars).
    // The main loop also caps the frame rate (game.cpp) in case the driver ignores this.
    const char* vs = getenv( "SHAR_VSYNC" );            // SHAR_VSYNC=0 turns it off (debugging)
    if( SDL_GL_SetSwapInterval( ( vs && vs[0] == '0' ) ? 0 : 1 ) != 0 )
        fprintf(stderr, "PDDI: vsync unavailable (%s); relying on the frame cap\n", SDL_GetError());

    extern int g_pglMaxTextureSize;          // cached for texture creation on loader threads (gltex.cpp)
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &g_pglMaxTextureSize);

    CreateWorkerContexts(window, glContext);

    int w = init->xsize, h = init->ysize;
    if(mode == PDDI_DISPLAY_FULLSCREEN || mode == PDDI_DISPLAY_FULLSCREEN_DESKTOP)
    {
        SDL_GL_GetDrawableSize(window, &w, &h);     // SwapBuffers keeps following it afterwards
    }
    return InitDisplay(w, h, init->bpp);
}

bool pglDisplay::InitDisplay(int x, int y, int bpp)
{
    reset = true;

    winWidth = x;
    winHeight = y;

    char* glVendor   = (char*)glGetString(GL_VENDOR);
    char* glRenderer = (char*)glGetString(GL_RENDERER);
    char* glVersion  = (char*)glGetString(GL_VERSION);
    char* glExtensions = (char*)glGetString(GL_EXTENSIONS);

    extBGRA = CheckExtension("GL_EXT_bgra");

    //sprintf(userDisplayInfo[0].description,"OpenGL - Vendor: %s, Renderer: %s, Version: %s",glVendor,glRenderer,glVersion);

    return true;
}

extern pddiDisplayInfo displayInfo[2];

pddiDisplayInfo* pglDisplay::GetDisplayInfo(void)
{
    return &displayInfo[0];
}

unsigned pglDisplay::GetFreeTextureMem()
{
    return unsigned(-1);
}

unsigned pglDisplay::GetBufferMask()
{
    return ~0U;
}

int pglDisplay::GetHeight()
{
    return winHeight;
}

int pglDisplay::GetWidth()
{
    return winWidth;
}

int pglDisplay::GetDepth()
{
    return winBitDepth;
}

pddiDisplayMode pglDisplay::GetDisplayMode(void)
{
    return mode;
}

int pglDisplay::GetNumColourBuffer(void)
{
    return 2;
}

void pglDisplay::GetGamma(float* r, float* g, float* b)
{
    *r = gammaR;
    *g = gammaG;
    *b = gammaB;
}

void pglDisplay::SetGamma(float r, float g, float b)
{
    gammaR = r;
    gammaG = g;
    gammaB = b;
}

// Debug aid (Linux port): SHAR_SHOT_EVERY=<seconds> saves the back buffer as shot_<n>.png in the working
// directory every <seconds>. Uncompressed PNG (stored deflate blocks), so no extra libraries are needed.
static unsigned Crc32(const unsigned char* d, size_t n, unsigned c = 0xffffffffu)
{
    for(size_t i = 0; i < n; i++)
    {
        c ^= d[i];
        for(int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return c;
}

static void PutBE(FILE* f, unsigned v) { unsigned char b[4] = { (unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v }; fwrite(b, 1, 4, f); }

static void WriteChunk(FILE* f, const char* type, const unsigned char* data, unsigned len)
{
    PutBE(f, len);
    unsigned char* buf = (unsigned char*)malloc(len + 4);
    memcpy(buf, type, 4);
    if(len) memcpy(buf + 4, data, len);
    fwrite(buf, 1, len + 4, f);
    PutBE(f, Crc32(buf, len + 4) ^ 0xffffffffu);
    free(buf);
}

static void SavePNG(const char* path, int w, int h, const unsigned char* rgbaBottomUp)
{
    FILE* f = fopen(path, "wb");
    if(!f) return;
    static const unsigned char sig[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    unsigned char ihdr[13] = { 0 };
    ihdr[0] = w >> 24; ihdr[1] = w >> 16; ihdr[2] = w >> 8; ihdr[3] = w;
    ihdr[4] = h >> 24; ihdr[5] = h >> 16; ihdr[6] = h >> 8; ihdr[7] = h;
    ihdr[8] = 8; ihdr[9] = 2;                                  // 8-bit RGB
    WriteChunk(f, "IHDR", ihdr, 13);

    size_t rowLen = 1 + (size_t)w * 3, rawLen = rowLen * h;
    unsigned char* raw = (unsigned char*)malloc(rawLen);
    for(int y = 0; y < h; y++)
    {
        const unsigned char* src = rgbaBottomUp + (size_t)(h - 1 - y) * w * 4;
        unsigned char* dst = raw + y * rowLen;
        *dst++ = 0;
        for(int x = 0; x < w; x++) { *dst++ = src[0]; *dst++ = src[1]; *dst++ = src[2]; src += 4; }
    }
    size_t blocks = (rawLen + 65534) / 65535;
    size_t zlen = 2 + rawLen + blocks * 5 + 4;
    unsigned char* z = (unsigned char*)malloc(zlen);
    unsigned char* zp = z;
    *zp++ = 0x78; *zp++ = 0x01;
    unsigned a = 1, b = 0;
    for(size_t off = 0; off < rawLen; off += 65535)
    {
        unsigned n = (unsigned)((rawLen - off) < 65535 ? (rawLen - off) : 65535);
        *zp++ = (off + n >= rawLen) ? 1 : 0;
        *zp++ = n & 255; *zp++ = n >> 8; *zp++ = ~n & 255; *zp++ = (~n >> 8) & 255;
        memcpy(zp, raw + off, n); zp += n;
        for(unsigned i = 0; i < n; i++) { a = (a + raw[off + i]) % 65521; b = (b + a) % 65521; }
    }
    unsigned adler = (b << 16) | a;
    *zp++ = adler >> 24; *zp++ = adler >> 16; *zp++ = adler >> 8; *zp++ = adler;
    WriteChunk(f, "IDAT", z, (unsigned)(zp - z));
    WriteChunk(f, "IEND", NULL, 0);
    fclose(f);
    free(z);
    free(raw);
}

static void MaybeScreenshot(int w, int h)
{
    static int every = -1;
    static unsigned last = 0;
    static int n = 0;
    if(every < 0)
    {
        const char* e = getenv("SHAR_SHOT_EVERY");
        every = e ? atoi(e) : 0;
    }
    if(every <= 0) return;
    unsigned now = SDL_GetTicks();
    if(now - last < (unsigned)every * 1000) return;
    last = now;
    unsigned char* px = (unsigned char*)malloc((size_t)w * h * 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    char path[64];
    sprintf(path, "shot_%03d.png", n++);
    SavePNG(path, w, h, px);
    free(px);
}

void pglDisplay ::SwapBuffers()
{
    MaybeScreenshot( winWidth, winHeight );
    SDL_GL_SwapWindow( window );
    reset = false;      // as the Win32 display: otherwise every BeginFrame treats the device as reset and
                        // re-uploads (and leaks) every texture

    // the window is resizable: follow its size (viewports and aspect ratios are worked out from it each frame)
    int w, h;
    SDL_GL_GetDrawableSize( window, &w, &h );
    if( w > 0 && h > 0 )
    {
        winWidth = w;
        winHeight = h;
    }
}

unsigned pglDisplay::Screenshot(pddiColour* buffer, int nBytes)
{
    // not implemented under linux
    assert(0 && "PDDI: pddiDisplay::ScreenShot() - Not implemented under linux.");
    return 0;
}

// SDL2 replacement for EnumDisplaySettings. The game offers fixed 4:3 resolutions (640x480..1600x1200)
// that modern monitors often do not list as modes; any size that fits the desktop works in a window,
// so those are reported (16 and 32 bpp, like Win32) plus the display's real fullscreen modes.
static void AddMode(pddiModeInfo* modes, int& n, int w, int h, int bpp)
{
    for(int i = 0; i < n; i++)
        if(modes[i].width == w && modes[i].height == h && modes[i].bpp == bpp)
            return;
    if(n >= 1024)
        return;
    modes[n].width = w;
    modes[n].height = h;
    modes[n].bpp = bpp;
    n++;
}

unsigned pglDisplay::FillDisplayModes(pddiModeInfo* displayModes)
{
    int nModes = 0;
    if(!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_InitSubSystem(SDL_INIT_VIDEO);

    SDL_DisplayMode desktop;
    int dw = 1600, dh = 1200;
    if(SDL_GetDesktopDisplayMode(0, &desktop) == 0)
    {
        dw = desktop.w;
        dh = desktop.h;
    }

    // every Win32Platform::Resolution (incl. the port's widescreen additions) that fits the desktop
    static const int gameRes[][2] = { {640,480}, {800,600}, {1024,768}, {1152,864}, {1280,1024}, {1600,1200},
                                      {1280,720}, {1920,1080}, {2560,1440}, {2560,1600} };
    for(int r = 0; r < (int)(sizeof(gameRes) / sizeof(gameRes[0])); r++)
    {
        if(gameRes[r][0] <= dw && gameRes[r][1] <= dh)
        {
            AddMode(displayModes, nModes, gameRes[r][0], gameRes[r][1], 16);
            AddMode(displayModes, nModes, gameRes[r][0], gameRes[r][1], 32);
        }
    }

    for(int m = 0; m < SDL_GetNumDisplayModes(0); m++)
    {
        SDL_DisplayMode mode;
        if(SDL_GetDisplayMode(0, m, &mode) == 0)
        {
            AddMode(displayModes, nModes, mode.w, mode.h, 16);
            AddMode(displayModes, nModes, mode.w, mode.h, 32);
        }
    }
    return nModes;
}
    
void pglDisplay::BeginTiming()
{
    gettimeofday(&beginTime,NULL);
}

float pglDisplay::EndTiming()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    // return time in milliseconds
    return (float)(tv.tv_sec - beginTime.tv_sec) * 1e3 + (float)(tv.tv_usec - beginTime.tv_usec) * 1e-3;
}

void pglDisplay::BeginContext(void)
{
}

void pglDisplay::EndContext(void)
{
}

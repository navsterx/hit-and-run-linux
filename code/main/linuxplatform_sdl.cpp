//===========================================================================
// Linux port: SDL2 replacements for the Win32 window code in win32platform.cpp.
//
// Win32Platform is the game's PC platform class; everything in it is shared with Linux except the window
// creation, the window procedure (WndProc) and the mouse/cursor helpers, which live here instead.
// PumpEvents() is called by the game loop where the Win32 build ran its PeekMessage loop, and does what
// WndProc did: focus handling, front-end mouse movement and clicks, and quitting when the window closes.
//===========================================================================

#ifdef RAD_LINUX

#include <SDL2/SDL.h>
#include <stdio.h>

#include <radcontroller.hpp>
#include <p3d/platform.hpp>
#include <p3d/context.hpp>
#include <input/inputmanager.h>
#include <input/FEMouse.h>
#include <input/MouseCursor.h>
#include <main/win32platform.h>
#include <render/RenderFlow/renderflow.h>

static const char ApplicationName[] = "The Simpsons: Hit & Run";
static const Win32Platform::Resolution StartingResolution = Win32Platform::Res_800x600;   // as win32platform.cpp

//==============================================================================
// Win32Platform::InitializeWindow
//==============================================================================
// Creates the game window. The GL context itself is created by the PDDI display (gldisplay.cpp) on this
// window, so the GL attributes must be set here, before the window exists.
bool Win32Platform::InitializeWindow( HINSTANCE hInstance )
{
    mhInstance = hInstance;

    // relative mouse mode (mouse-look in gameplay): use raw relative input, not pointer warping, which
    // reports a spurious one-pixel motion every frame under XWayland
    SDL_SetHint( SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0" );

    if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC ) != 0 )
    {
        fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
        return false;
    }

    fprintf( stderr, "SDL video driver: %s\n", SDL_GetCurrentVideoDriver( ) );

    // Pure3D's GL renderer is fixed-function OpenGL 1.x: ask for a compatibility context.
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
    SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );

    int x, y;
    TranslateResolution( StartingResolution, x, y );

    SDL_Window* window = SDL_CreateWindow( ApplicationName,
                                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           x, y,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN );
    if( window == NULL )
    {
        fprintf( stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError() );
        return false;
    }

    mhWnd = (HWND) window;
    ShowTheCursor( false );
    return true;
}

//==============================================================================
// Win32Platform::ShowGameWindow
//==============================================================================
void Win32Platform::ShowGameWindow()
{
    SDL_ShowWindow( (SDL_Window*) mhWnd );
    SDL_RaiseWindow( (SDL_Window*) mhWnd );
}

//==============================================================================
// Win32Platform::GetDesktopSize
//==============================================================================
void Win32Platform::GetDesktopSize( int& width, int& height )
{
    SDL_DisplayMode mode;
    if( SDL_GetDesktopDisplayMode( 0, &mode ) == 0 )
    {
        width = mode.w;
        height = mode.h;
    }
    else
    {
        width = 1024;
        height = 768;
    }
}

//==============================================================================
// Win32Platform::ResizeWindow
//==============================================================================
void Win32Platform::ResizeWindow()
{
    if( mFullscreen )
    {
        return;
    }

    int cx, cy;
    TranslateResolution( mResolution, cx, cy );

    SDL_Window* window = (SDL_Window*) mhWnd;
    SDL_SetWindowSize( window, cx, cy );
    SDL_SetWindowPosition( window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED );
    SDL_ShowWindow( window );
}

//==============================================================================
// Win32Platform::ShowTheCursor
//==============================================================================
void Win32Platform::ShowTheCursor( bool show )
{
    if( mShowCursor != show )
    {
        mShowCursor = show;
        SDL_ShowCursor( mShowCursor ? SDL_ENABLE : SDL_DISABLE );
    }
}

//==============================================================================
// Win32Platform::PumpEvents
//==============================================================================
// The Linux equivalent of the Win32 message loop + WndProc. Keyboard and gamepad input is read by the
// input layer (as it was through DirectInput), so only window-level events are handled here.
bool Win32Platform::PumpEvents()
{
    bool quit = false;
    SDL_Event e;
    while( SDL_PollEvent( &e ) )
    {
        radControllerSDLHandleEvent( &e );                 // wheel + controller hot-plug for radcore input

        InputManager* pInputManager = GetInputManager();
        FEMouse* pFEMouse = pInputManager ? pInputManager->GetFEMouse() : NULL;

        switch( e.type )
        {
        case SDL_QUIT:
            quit = true;                                    // WM_DESTROY -> WM_QUIT
            break;

        case SDL_WINDOWEVENT:
            switch( e.window.event )
            {
            case SDL_WINDOWEVENT_FOCUS_GAINED:              // WM_ACTIVATEAPP (TRUE)
            case SDL_WINDOWEVENT_FOCUS_LOST:                // WM_ACTIVATEAPP (FALSE)
                {
                    bool active = ( e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED );
                    if( spInstance != NULL && spInstance->mpContext != NULL )
                    {
                        if( active )
                        {
                            RenderFlow* rf = GetRenderFlow();
                            rf->SetGamma( rf->GetGamma() );
                        }
                        if( pFEMouse )
                        {
                            pFEMouse->SetInGameOverride( !active );
                        }
                    }
                }
                break;

            case SDL_WINDOWEVENT_LEAVE:                     // WM_MOUSELEAVE
                if( pFEMouse )
                {
                    pFEMouse->getCursor()->SetVisible( false );
                }
                break;
            }
            break;

        case SDL_MOUSEMOTION:                               // WM_MOUSEMOVE
            if( pFEMouse && pFEMouse->DidWeMove( e.motion.x, e.motion.y ) )
            {
                int w, h;
                SDL_GetWindowSize( (SDL_Window*) mhWnd, &w, &h );
                pFEMouse->Move( e.motion.x, e.motion.y, w, h );
            }
            ShowTheCursor( false );
            break;

        case SDL_MOUSEBUTTONDOWN:                           // WM_LBUTTONDOWN
            if( pFEMouse && e.button.button == SDL_BUTTON_LEFT )
            {
                pFEMouse->ButtonDown( BUTTON_LEFT );
            }
            break;

        case SDL_MOUSEBUTTONUP:                             // WM_LBUTTONUP
            if( pFEMouse && e.button.button == SDL_BUTTON_LEFT )
            {
                pFEMouse->ButtonUp( BUTTON_LEFT );
            }
            break;

        default:
            break;
        }
    }
    return quit;
}

#endif // RAD_LINUX

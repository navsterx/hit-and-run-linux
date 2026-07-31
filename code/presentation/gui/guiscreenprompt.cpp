//===========================================================================
// Copyright (C) 2000 Radical Entertainment Ltd.  All rights reserved.
//
// Component:   CGuiScreenPrompt
//
// Description: Implementation of the CGuiScreenPrompt class.
//
// Authors:     Tony Chu
//
// Revisions		Date			Author	    Revision
//                  2002/07/04      TChu        Created for SRR2
//
//===========================================================================

//===========================================================================
// Includes
//===========================================================================
#include <presentation/gui/guiscreenprompt.h>
#include <presentation/gui/guiscreenmessage.h>
#include <presentation/gui/guimenu.h>
#include <presentation/gui/guiscreenmemorycard.h>
#include <presentation/gui/utility/specialfx.h>

#include <memory/srrmemory.h>
#include <mission/gameplaymanager.h>
#include <mission/mission.h>
#include <gameflow/gameflow.h>

#include <p3d/unicode.hpp>
#include <raddebug.hpp> // Foundation
#include <Layer.h>
#include <Page.h>
#include <Screen.h>
#include <Text.h>
#include <Sprite.h>
#include <Group.h>
#include <Pure3dObject.h>

//===========================================================================
// Global Data, Local Data, Local Classes
//===========================================================================

int CGuiScreenPrompt::s_messageIndex = 0;
int CGuiScreenPrompt::s_numResponses = 0;
CGuiMenuPrompt::ePromptResponse CGuiScreenPrompt::s_responses[ CGuiMenuPrompt::MAX_NUM_RESPONSES ];
CGuiEntity* CGuiScreenPrompt::s_pPromptCallback = NULL;
bool CGuiScreenPrompt::s_defaultToNo = true;

const float ERROR_PROMPT_FADE_TIME = 100.0f;

// Linux port: the GenericPrompt pages compiled into the PC data (retail and the dump's) are older than
// ePromptQuestion: strings are missing (overwrite, delete corrupt, progressive scan, ...) and the retail
// one has two resolution prompts more, so an enum value picked the wrong string (a bonus mission's
// "unlocked <car>" showed another message). Look each string up by its text bible ID instead.
static const char* const PROMPT_STRING_IDS[] =
{
    "MSG_CONFIRM_QUIT",
    "MSG_CONFIRM_SAVE_BEFORE_QUIT",
    "MSG_CONFIRM_RESTART",
    "MSG_CONFIRM_ABORT",
    "MSG_CONFIRM_RACE_AGAIN",
    "MSG_CONFIRM_NEW_GAME",
    "MSG_CONFIRM_START_MISSION",
    "MSG_LOAD_CONFIRM_(GC)",
    "MSG_LOAD_CONFIRM_(PS2)",
    "MSG_LOAD_CONFIRM_(XBOX)",
    "MSG_SAVE_CONFIRM_(GC)",
    "MSG_SAVE_CONFIRM_(PS2)",
    "MSG_SAVE_CONFIRM_(XBOX)",
    "MSG_SAVE_CONFIRM_OVERWRITE_(GC)",
    "MSG_SAVE_CONFIRM_OVERWRITE_(PS2)",
    "MSG_SAVE_CONFIRM_OVERWRITE_(XBOX)",
    "MSG_LOAD_SUCCESSFUL",
    "MSG_SAVE_SUCCESSFUL",
    "MSG_FORMAT_CONFIRM_(GC)",
    "MSG_FORMAT_CONFIRM_(PS2)",
    "MSG_FORMAT_CONFIRM_(XBOX)",
    "MSG_FORMAT_CONFIRM2_(GC)",
    "MSG_FORMAT_CONFIRM2_(PS2)",
    "MSG_FORMAT_CONFIRM2_(XBOX)",
    "MSG_FORMAT_SUCCESS_(GC)",
    "MSG_FORMAT_SUCCESS_(PS2)",
    "MSG_FORMAT_SUCCESS_(XBOX)",
    "MSG_FORMAT_FAIL_(GC)",
    "MSG_FORMAT_FAIL_(PS2)",
    "MSG_FORMAT_FAIL_(XBOX)",
    "MSG_DELETE_CORRUPT_SUCCESS_(GC)",
    "MSG_DELETE_CORRUPT_SUCCESS_(PS2)",
    "MSG_DELETE_CORRUPT_SUCCESS_(XBOX)",
    "MSG_DELETE_CORRUPT_FAIL_(GC)",
    "MSG_DELETE_CORRUPT_FAIL_(PS2)",
    "MSG_DELETE_CORRUPT_FAIL_(XBOX)",
    "MSG_LOAD_CARD_EMPTY_(GC)",
    "MSG_LOAD_CARD_EMPTY_(PS2)",
    "MSG_LOAD_CARD_EMPTY_(XBOX)",
    "MSG_LOAD_CARD_EMPTY_(XBOX_HD)",
#ifdef RAD_WIN32
    "MSG_LOAD_CARD_EMPTY_(PC)",
#endif
    "MSG_DISPLAY_PROGRESSIVE_SCAN",
    "MSG_DISPLAY_PROGRESSIVE_SCAN_PS2",
    "MSG_DISPLAY_PROGRESSIVE_SCAN_CONFIRM",
    "MSG_DISPLAY_PROGRESSIVE_SCAN_TEST",
    "MSG_DELETE_CORRUPT_(GC)",
    "MSG_DELETE_CORRUPT_(PS2)",
    "MSG_DELETE_CORRUPT_(XBOX)",
    "MSG_DELETE_CORRUPT_(XBOX_HD)",
    "MSG_HARDDISK_FULL_(XBOX)",
#ifdef RAD_WIN32
    "MSG_CONFIRM_RESTOREALLDEFAULTS",
    "MSG_CONFIRM_QUIT_TO_SYSTEM",
    "MSG_CONFIRM_SAVE_BEFORE_QUIT", // PROMPT_CONFIRM_SAVE_BEFORE_QUITTOSYSTEM
#endif
};
// PROMPT_STRING_IDS must follow ePromptQuestion
typedef char PromptStringIdsMatchEnum[ sizeof( PROMPT_STRING_IDS ) / sizeof( PROMPT_STRING_IDS[ 0 ] ) == NUM_PROMPT_QUESTIONS ? 1 : -1 ];

// page string index for a prompt (or, past NUM_PROMPT_QUESTIONS, an in-game message); unchanged if
// the page lacks that string
static int FindPromptString( Scrooby::Text* text, int messageIndex )
{
    char ingameId[ 32 ];
    const char* id = NULL;
    if( messageIndex < NUM_PROMPT_QUESTIONS )
    {
        id = PROMPT_STRING_IDS[ messageIndex ];
    }
    else
    {
        sprintf( ingameId, "INGAME_MESSAGE_%02d", messageIndex - NUM_PROMPT_QUESTIONS );
        id = ingameId;
    }

    for( int i = 0; i < text->GetNumOfStrings(); i++ )
    {
        const char* pageId = text->GetStringId( i );
        if( pageId != NULL && strcmp( pageId, id ) == 0 )
        {
            return i;
        }
    }
    return messageIndex;
}

//===========================================================================
// Public Member Functions
//===========================================================================

//===========================================================================
// CGuiScreenPrompt::CGuiScreenPrompt
//===========================================================================
// Description: Constructor.
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
CGuiScreenPrompt::CGuiScreenPrompt
(
	Scrooby::Screen* pScreen,
	CGuiEntity* pParent,
    eGuiWindowID windowID
)
:   CGuiScreen( pScreen, pParent, windowID ),
    m_pMenu( NULL ),
    m_messageIcon( NULL ),
    m_elapsedTime( 0 ),
    m_xboxDashboardLabel( NULL ),
    m_promptMessage( NULL ),
    m_messageIndex( 0 ),
    m_tvFrame( NULL )
{
MEMTRACK_PUSH_GROUP( "GUIScreenPrompt" );
    if( m_screenCover != NULL )
    {
        m_screenCover->SetAlpha( 0.0f );
        m_screenCover = NULL;
    }

    m_originalStringBuffer[ 0 ] = '\0';

    // Create a menu.
    //
    Scrooby::Page* pPage = m_pScroobyScreen->GetPage( "Prompt" );
    m_pMenu = new CGuiMenuPrompt( this, pPage );
    rAssert( m_pMenu != NULL );

    // TC: quick & dirty hack, no time to make this pretty!
    //
    m_pMenu->GetMenuItem( 0 )->m_defaultColour = tColour( 255, 255, 255 );

    if( windowID == GUI_SCREEN_ID_ERROR_PROMPT )
    {
	    pPage = m_pScroobyScreen->GetPage( "ErrorPrompt" );
	    rAssert( pPage != NULL );

//        m_messageIcon = pPage->GetSprite( "ErrorIcon" );
//        rAssert( m_messageIcon );

        m_xboxDashboardLabel = pPage->GetGroup( "XboxDashboard" );
        rAssert( m_xboxDashboardLabel != NULL );
        m_xboxDashboardLabel->SetVisible( false );

        this->SetFadeTime( ERROR_PROMPT_FADE_TIME );

#ifdef RAD_XBOX
        // wrap "Go to Xbox Dashboard" text
        //
        Scrooby::Group* xboxDashboard = pPage->GetGroup( "XboxDashboard" );
        rAssert( xboxDashboard != NULL );

        Scrooby::Text* pText = xboxDashboard->GetText( "GotoDashboard" );
        if( pText != NULL )
        {
            pText->SetTextMode( Scrooby::TEXT_WRAP );
        }
#endif
    }
    else
    {
        this->SetSlidingEnabled( SCREEN_FX_SLIDE_Y, true );

	    pPage = m_pScroobyScreen->GetPage( "GenericPrompt" );
	    rAssert( pPage != NULL );
    }

    Scrooby::Page* messageBoxPage = m_pScroobyScreen->GetPage( "MessageBox" );
    if( messageBoxPage != NULL )
    {
        this->AutoScaleFrame( messageBoxPage );

        Scrooby::Sprite* messageIcon = messageBoxPage->GetSprite( "ErrorIcon" );
        if( messageIcon != NULL )
        {
            messageIcon->ResetTransformation();
            messageIcon->ScaleAboutCenter( MESSAGE_ICON_CORRECTION_SCALE );
        }
    }

    m_promptMessage = pPage->GetText( "Message" );
    rAssert( m_promptMessage );
    m_promptMessage->SetTextMode( Scrooby::TEXT_WRAP );
    m_promptMessage->ResetTransformation();
    m_promptMessage->ScaleAboutCenter(0.9f);


	pPage = m_pScroobyScreen->GetPage( "TVFrame" );
	if( pPage != NULL )
    {
        m_tvFrame = pPage->GetLayer( "TVFrame" );
        Scrooby::Sprite* tvFrame = pPage->GetSprite( "TVFrame" );
        if( tvFrame != NULL )
        {
            tvFrame->SetStretchToWindow( true );    // Linux port: cover a widescreen window
        }
    }
MEMTRACK_POP_GROUP( "GUIScreenPrompt" );
}


//===========================================================================
// CGuiScreenPrompt::~CGuiScreenPrompt
//===========================================================================
// Description: Destructor.
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
CGuiScreenPrompt::~CGuiScreenPrompt()
{
    if( m_pMenu != NULL )
    {
        delete m_pMenu;
        m_pMenu = NULL;
    }
}


//===========================================================================
// CGuiScreenPrompt::HandleMessage
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenPrompt::HandleMessage
(
	eGuiMessage message, 
	unsigned int param1,
	unsigned int param2 
)
{
    if( m_state == GUI_WINDOW_STATE_RUNNING )
    {
        switch( message )
        {
#ifdef RAD_PS2
            case GUI_MSG_CONTROLLER_START:
            {
                if ( GetGameFlow()->GetCurrentContext() == CONTEXT_PAUSE )
                    if ( s_pPromptCallback )
                        s_pPromptCallback->HandleMessage( GUI_MSG_PROMPT_START_RESPONSE );

                break;
            }
#endif
            case GUI_MSG_UPDATE:
            {
                m_elapsedTime += param1;

                // pulse message icon
                //
                float scale = GuiSFX::Pulse( (float)m_elapsedTime,
                                             500.0f,
                                             1.0f,
                                             0.1f );

                if( m_messageIcon != NULL )
                {
                    m_messageIcon->ResetTransformation();
                    m_messageIcon->ScaleAboutCenter( scale );
                }

                if (!m_pMenu->HasSelectionBeenMade())
                {
                    rAssert( s_pPromptCallback );
                    s_pPromptCallback->HandleMessage( GUI_MSG_PROMPT_UPDATE, param1, param2);
                }

                break;
            }
            case GUI_MSG_CONTROLLER_BACK:
            {
#ifdef RAD_XBOX
                if( m_ID == GUI_SCREEN_ID_ERROR_PROMPT )
                {
                    // special case for going to xbox dashboard on the
                    // "full xbox hard disk" error message
                    //
                    rAssert( s_pPromptCallback );
                    rAssert( m_promptMessage );
                    s_pPromptCallback->HandleMessage( GUI_MSG_MENU_PROMPT_RESPONSE,
                                                      m_promptMessage->GetIndex(),
                                                      CGuiMenuPrompt::RESPONSE_MANAGE_MEMCARDS );
                }
#endif
                // ignore BACK inputs, thereby, not allowing users to back
                // out of prompt
                return;

                break;
            }

            case GUI_MSG_MENU_SELECTION_MADE:
            {
                rAssert( m_pMenu );
                CGuiMenuPrompt::ePromptResponse response = m_pMenu->GetResponse( param1 );

                rAssert( s_pPromptCallback );
                rAssert( m_promptMessage );
                enum eGuiMessage message;
                if (m_ID==GUI_SCREEN_ID_GENERIC_PROMPT)
                    message = GUI_MSG_MENU_PROMPT_RESPONSE;
                else if (m_ID==GUI_SCREEN_ID_ERROR_PROMPT)
                    message = GUI_MSG_ERROR_PROMPT_RESPONSE;
                else
                {
                    message = GUI_MSG_MENU_PROMPT_RESPONSE;
                    rTuneAssert(!"not reached");
                }
                s_pPromptCallback->HandleMessage( message,
                                                  m_messageIndex,
                                                  response );

                break;
            }

            default:
            {
                break;
            }
        }

        // relay message to menu
        if( m_pMenu != NULL )
        {
            m_pMenu->HandleMessage( message, param1, param2 );
        }
    }
    else if( m_state == GUI_WINDOW_STATE_OUTRO )
    {
        // TC: This is a nasty hack! GOD, please forgive me, as this is the
        //     easiest thing for me to do w/ just a few days left before beta.
        //
        if( m_ID == GUI_SCREEN_ID_GENERIC_PROMPT && s_messageIndex == PROMPT_CONFIRM_NEW_GAME )
        {
            rAssert( m_p3dObject != NULL );

            tMultiController* multiController = m_p3dObject->GetMultiController();
            if( multiController != NULL )
            {
                const float NUM_FADE_OUT_FRAMES = 15.0f;
                float currentFrame = multiController->GetFrame();
                float numRemainingFrames = multiController->GetNumFrames() - currentFrame;

                if( numRemainingFrames < NUM_FADE_OUT_FRAMES )
                {
                    // fade out TV frame
                    //
                    if( m_tvFrame != NULL )
                    {
                        float alpha = numRemainingFrames / NUM_FADE_OUT_FRAMES;

                        // decrease fade rate for low alpha values
                        alpha *= alpha;

                        if( alpha > 0.0f && alpha < 1.0f )
                        {
                            m_tvFrame->SetAlpha( alpha );
                        }
                        else
                        {
                            m_tvFrame->SetAlpha( 0.0f );
                        }
                    }

                    // TC [HACK]: To prevent any clipping in homer's mouth
                    //            in the last few frames.
                    //
                    if( numRemainingFrames < 1.0f )
                    {
                        this->RestoreScreenCover();
                    }
                }
            }
        }

        if( m_numTransitionsPending < 0 )
        {
            // restore original string buffer
            //
            if( m_originalStringBuffer[ 0 ] != '\0' )
            {
                rAssert( m_promptMessage != NULL );
                UnicodeChar* stringBuffer = m_promptMessage->GetStringBuffer();
                rAssert( stringBuffer != NULL );

                p3d::UnicodeStrCpy( static_cast<P3D_UNICODE*>( m_originalStringBuffer ),
                                    static_cast<P3D_UNICODE*>( stringBuffer ),
                                    sizeof( m_originalStringBuffer ) );

                m_originalStringBuffer[ 0 ] = '\0';
            }
        }
    }

	// Propogate the message up the hierarchy.
	//
	CGuiScreen::HandleMessage( message, param1, param2 );
}


//===========================================================================
// CGuiScreenPrompt::Display
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenPrompt::Display( int messageIndex,
                                CGuiEntity* pCallback,
                                int numResponses,
                                CGuiMenuPrompt::ePromptResponse* responses )
{
    s_messageIndex = messageIndex;
    s_pPromptCallback = pCallback;
    s_numResponses = numResponses;

    for( int i = 0; i < numResponses; i++ )
    {
        s_responses[ i ] = responses[ i ];
    }
}


//===========================================================================
// CGuiScreenPrompt::InitIntro
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenPrompt::InitIntro()
{
    rAssert( m_promptMessage );
    m_messageIndex = s_messageIndex;
    int stringIndex = s_messageIndex;
    if( m_ID != GUI_SCREEN_ID_ERROR_PROMPT )
    {
        stringIndex = FindPromptString( m_promptMessage, s_messageIndex );
    }
    rAssert( stringIndex >= 0 &&
             stringIndex < m_promptMessage->GetNumOfStrings() );

#ifdef RAD_XBOX
    if( m_ID == GUI_SCREEN_ID_ERROR_PROMPT )
    {
        // special case for "full xbox hard disk" error message
        //
        bool showLabel = (s_messageIndex == GetErrorMessageIndex( NoFreeSpace, ERROR_DURING_CHECKING ));
        rAssert( m_xboxDashboardLabel != NULL );
        m_xboxDashboardLabel->SetVisible( showLabel );
    }
#endif

    m_promptMessage->SetIndex( stringIndex );

    CGuiScreenMessage::FormatMessage( m_promptMessage,
                                        m_originalStringBuffer,
                                        sizeof( m_originalStringBuffer ) );

//        rAssert( m_messageIcon );
//        m_messageIcon->ResetTransformation();

    rAssert( m_pMenu );
    m_pMenu->SetNumResponses( s_numResponses );
    m_pMenu->Reset();

    for( int i = 0; i < s_numResponses; i++ )
    {
        m_pMenu->SetResponse( i, s_responses[ i ] );

        if( s_defaultToNo && s_responses[ i ] == CGuiMenuPrompt::RESPONSE_NO )
        {
            // set NO as default response
            //
            m_pMenu->Reset( i );
        }
    }

    m_elapsedTime = 0;
}


//===========================================================================
// CGuiScreenPrompt::InitRunning
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenPrompt::InitRunning()
{
}


//===========================================================================
// CGuiScreenPrompt::InitOutro
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenPrompt::InitOutro()
{
}


//---------------------------------------------------------------------
// Private Functions
//---------------------------------------------------------------------


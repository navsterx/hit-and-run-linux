
//===========================================================================
// Copyright (C) 2000 Radical Entertainment Ltd.  All rights reserved.
//
// Component:   Scrooby Runtime Public Interface
//
// Description: This contains the Drawable Base class.
//
// Authors:     Wilkin Ng, Ian Gipson, Mike Perzel( 12/20/2001 )
//
//===========================================================================

//===========================================================================
// Includes
//===========================================================================
#include "FePolygon.h"
#include <p3d/shader.hpp>
#include <p3d/utility.hpp>
#include <p3d/displayaspect.hpp>
#include <raddebug.hpp>
#include "utility/debugMessages.h"

//===========================================================================
// Global Data, Local Data, Local Classes
//===========================================================================

//===========================================================================
// Member Functions
//===========================================================================

//===========================================================================
// FePolygon::FePolygon
//===========================================================================
// Description: constructor
//
// Constraints:    NONE
//
// Parameters:    name - the name to be passed up to the FeDrawable
//
// Return:      Nothing
//
//===========================================================================
FePolygon::FePolygon( const tName& name ) : FeDrawable( name )
{
    Reset();

    mShader = new tShader();
    mShader->AddRef();
    mShader->SetInt( PDDI_SP_BLENDMODE, PDDI_BLEND_ALPHA );
    mShader->SetInt( PDDI_SP_SHADEMODE, PDDI_SHADE_GOURAUD );
#ifdef RAD_PS2
    if( p3d::display->GetDepth() == 16 )
    {
        mShader->SetInt( PDDI_SP_ALPHATEST, 1 );   //turn on alpha testing     (only required for PS2 16 bit framebuffers )
    }
#endif
}

//===========================================================================
// FePolygon::FePolygon
//===========================================================================
// Description: constructor
//
// Constraints:    NONE
//
// Parameters:    NONE
//
// Return:      NONE
//
//===========================================================================
FePolygon::~FePolygon()
{
    mShader->Release();
    mShader = NULL;
}

//===========================================================================
// Reset
//===========================================================================
// Description: called by the constructor, this resets all the values
//
// Constraints:    NONE
//
// Parameters:    NONE
//
// Return:      NONE
//
//===========================================================================
void FePolygon::Reset()
{
    FeDrawable::Reset();

    mNumVertex = 0;
}

//===========================================================================
// SetNumVertex
//===========================================================================
// Description: called when creating the polygon - specifies the number of 
//              vertexes
//
// Constraints:    n must be >3 and less than or equal to the maximum allowed 
//              number of vertexes
//
// Parameters:    n - the number of vertexes for this polygon
//
// Return:      NONE
//
//===========================================================================
void FePolygon::SetNumVertex( int n )
{
    rAssert( n >= 3 && n <= MAX_POLYVERTEX );
    mNumVertex = n;
}

//===========================================================================
// SetVertexLocation(int which,int left,int bottom)
//===========================================================================
// Description: changes the location of the specified vertex
//
// Constraints:    NONE
//
// Parameters:    which - index of the vertex to change
//              left - the X position of the vertex
//              bottom - the Y position of the vertex
//
// Return:      NONE
//
//===========================================================================
void FePolygon::SetVertexLocation( int which, int left, int bottom )
{
    rAssert( which >= 0 && which < MAX_POLYVERTEX );

    float y;
    float x;
    NormalizeCoord( left, bottom, x, y );
    mVertex[ which ] = pddiVector( x, y, 0 );
}


void FePolygon::SetVertexColour( int which, int red, int green, int blue )
{
    rAssert( ( which >= 0 ) && ( which < MAX_POLYVERTEX ) );

    mVertexColour[ which ].Set( red, green, blue, mVertexColour[ which ].Alpha() );
    RecalculateDisplayColour( which );
}

void FePolygon::SetVertexColour( const unsigned int which, const tColour c )
{
    rAssert( ( which >= 0 ) && ( which < MAX_POLYVERTEX ) );
    mVertexColour[ which ] = c;
    RecalculateDisplayColour( which );
}

void FePolygon::SetVertexAlpha( int which, float alpha )
{
    rAssert( ( which >= 0 ) && ( which < MAX_POLYVERTEX ) );
    rAssert( ( alpha >= 0 ) && ( alpha <= 1.0f ) );

    int valueToSet = static_cast< int >( alpha * 255 );

    mVertexColour[ which ].Set
    ( 
        mVertexColour[ which ].Red(),
        mVertexColour[ which ].Green(),
        mVertexColour[ which ].Blue(),
        valueToSet 
    );
    RecalculateDisplayColour( which );
}

//===========================================================================
// GetBoundingBox
//===========================================================================
// Description: gets the extents of the bounding box
//
// Constraints:    
//
// Parameters:    xMin - the smallext x value
//              yMin - the smallest y value
//              xMax - the largest x value
//              yMax - the largest y value
//
// Return:      NONE
//
//===========================================================================
void FePolygon::GetBoundingBox( int& xMin, int& yMin, int& xMax, int& yMax ) const
{
    bool initialized = false;
    for( int i = 0; i < mNumVertex; i++ )
    {
        int x;
        int y;
        this->InverseNormalizeCoord
        ( 
            this->mVertex[ i ].x + FeDrawable::mPosX, 
            this->mVertex[ i ].y + FeDrawable::mPosY, 
            x, 
            y 
        );

        if( initialized == false )
        {
            xMin = x;
            xMax = x;
            yMin = y;
            yMax = y;
            initialized = true;
        }
        else
        {
            xMin = x < xMin ? x : xMin;
            xMax = x > xMax ? x : xMax;
            yMin = y < yMin ? y : yMin;
            yMax = y > yMax ? y : yMax;
        }
    }
}


//===========================================================================
// GetBoundingBoxSize
//===========================================================================
// Description: get the size of the bounding box
//
// Constraints:    None
//
// Parameters:    width - the width of the bounding box gets returned in here
//              height - the height of the bounding box gets returned in here
//
// Return:      NONE
//
//===========================================================================
void FePolygon::GetBoundingBoxSize( int& width, int& height ) const
{
    float minX = mVertex[ 0 ].x;
    float minY = mVertex[ 0 ].y;
    float maxX = mVertex[ 0 ].x;
    float maxY = mVertex[ 0 ].y;

    for( int i = 1; i < mNumVertex; i++ )
    {
        float x = mVertex[ i ].x;
        float y = mVertex[ i ].y;
        if( x < minX )
        {
            minX = x;
        }
        else if( x > maxX )
        {
            maxX = x;
        }

        if( y < minY )
        {
            minY = y;
        }
        else if( y > maxY )
        {
            maxY = y;
        }
    }

    int iMinX = 0;
    int iMaxX = 0;
    int iMinY = 0;
    int iMaxY = 0;

    this->InverseNormalizeCoord( minX, minY, iMinX, iMinY );
    this->InverseNormalizeCoord( maxX, maxY, iMaxX, iMaxY );

    width = iMaxX - iMinX;
    height = iMaxY - iMinY;
}

int FePolygon::GetNumOfVertexes()
{
    return mNumVertex;
}

//===========================================================================
// float FePolygon::GetVertexAlpha( const in which ) const
//===========================================================================
// Description: gets the current position of a vertex
//
// Constraints:    None
//
// Parameters:    which - the number of the vertex we care about
//
// Return:      the alpha value of that vertex
//
//===========================================================================
float FePolygon::GetVertexAlpha( const int which ) const
{
    rAssertMsg( which > 0, "FePolygon: tried to access polyogn vertex < 0" );
    rAssertMsg( which < MAX_POLYVERTEX, "FePolygon: tried to access polyogn vertex < max" );
    tColour color = mVertexColour[ which ];
    return color.Alpha() / 255.0f;
}


//===========================================================================
// tColour FePolygon::GetVertexColour( const int which ) const
//===========================================================================
// Description: gets the current position of a vertex
//
// Constraints:    None
//
// Parameters:    which - the number of the vertex we care about
//
// Return:      the color of that vertex
//
//===========================================================================
tColour FePolygon::GetVertexColour( const int which ) const
{
    rAssertMsg( which >= 0, "FePolygon: tried to access polyogn vertex < 0" );
    rAssertMsg( which < MAX_POLYVERTEX, "FePolygon: tried to access polyogn vertex > max" );
    return mVertexColour[ which ];
}

//===========================================================================
// void GetVertexLocation( const int which, int& left, int& bottom ) const
//===========================================================================
// Description: gets the current position of a vertex
//
// Constraints:    None
//
// Parameters:    which - the number of the vertex we care about
//              left - the horizontal position of the vertex
//              bottom - the vertical position of the vertex
//
// Return:      none
//
//===========================================================================
void FePolygon::GetVertexLocation( const int which, int& left, int& bottom ) const
{
    rAssert( ( which >= 0 ) && ( which < MAX_POLYVERTEX ) );

    float x;
    float y;

    x = mVertex[ which ].x;
    y = mVertex[ which ].y;
    InverseNormalizeCoord( x, y, left, bottom );
}

void FePolygon::Display()
{
    rAssert(mNumVertex >= 3 && mNumVertex <= MAX_POLYVERTEX);

    // TC [HACK]: Never allow polygon alpha to be greater than parent drawable alpha.
    //
    int polygonAlpha = GetVertexColour( 0 ).Alpha(); // assuming uniform alpha
                                                     // among all vertices
    int parentAlpha = GetColour().Alpha();
    if( polygonAlpha > parentAlpha )
    {
        SetAlpha( parentAlpha / 255.0f );
    }
    else
    {
        SetAlpha( polygonAlpha / 255.0f );
    }

#ifdef RAD_GAMECUBE
    pddiCullMode oldCullMode = p3d::pddi->GetCullMode();
    p3d::pddi->SetCullMode( PDDI_CULL_NONE );
#endif

    // Linux port: the page is drawn in the centred 4:3 area of the window, clipped to it (FeScreen). A
    // polygon spanning the page's full width or height (letterbox bars, fades, dimmed backgrounds) is
    // stretched to the window's edges on that axis and drawn unclipped. Page units: x 0-1, y 0-0.75.
    pddiVector vertex[ MAX_POLYVERTEX ];
    float minX = 1.0f, maxX = 0.0f, minY = 1.0f, maxY = 0.0f;
    int i;
    for( i = 0; i < mNumVertex; i++ )
    {
        vertex[ i ] = mVertex[ i ];
        float pageX = mVertex[ i ].x + FeDrawable::mPosX;
        float pageY = mVertex[ i ].y + FeDrawable::mPosY;
        minX = ( pageX < minX ) ? pageX : minX;
        maxX = ( pageX > maxX ) ? pageX : maxX;
        minY = ( pageY < minY ) ? pageY : minY;
        maxY = ( pageY > maxY ) ? pageY : maxY;
    }
    float stretchX = 1.0f / p3d::GetDesignScaleX();
    float stretchY = 1.0f / p3d::GetDesignScaleY();
    bool wide = ( stretchX > 1.0f ) && ( minX <= 0.01f ) && ( maxX >= 0.99f );
    bool tall = ( stretchY > 1.0f ) && ( minY <= 0.01f ) && ( maxY >= 0.74f );
    for( i = 0; i < mNumVertex; i++ )
    {
        if( wide )
        {
            float centre = 0.5f - FeDrawable::mPosX;    // the page centre in vertex coordinates
            vertex[ i ].x = centre + ( vertex[ i ].x - centre ) * stretchX;
        }
        if( tall )
        {
            float centre = 0.375f - FeDrawable::mPosY;
            vertex[ i ].y = centre + ( vertex[ i ].y - centre ) * stretchY;
        }
    }
    pddiRect pageScissor;
    if( wide || tall )
    {
        p3d::pddi->GetScissor( &pageScissor );
        pddiRect displayRect = p3d::GetDisplayRect();
        p3d::pddi->SetScissor( &displayRect );
    }

    pddiPrimStream* stream = p3d::pddi->BeginPrims( mShader->GetShader(), PDDI_PRIM_TRIANGLES, PDDI_V_C, mNumVertex > 3 ? mNumVertex+2 : mNumVertex );   

    // MIKE IMPROVE : this should be reorganized to allow for Tri-Stripping.  Currently does a manual tri-fan.
    for( i = 2; i < this->mNumVertex; i++ )
    {
        stream->Vertex( &vertex[ 0 ], mDisplayVertexColour[ 0 ] );
        stream->Vertex( &vertex[ i - 1 ], mDisplayVertexColour[ i - 1 ] );
        stream->Vertex( &vertex[ i ], mDisplayVertexColour[ i ] );
    }

    p3d::pddi->EndPrims(stream);

    if( wide || tall )
    {
        p3d::pddi->SetScissor( &pageScissor );
    }

    // TC [HACK]: Restore default polygon alpha.
    //
    if( polygonAlpha > parentAlpha )
    {
        SetAlpha( polygonAlpha / 255.0f );
    }

#ifdef RAD_GAMECUBE
    p3d::pddi->SetCullMode( oldCullMode );
#endif
}

void FePolygon::RecalculateDisplayColour( int which )
{
    int i = which > 0 ? which : 0;
    int end = which >= 0 ? which + 1 : MAX_POLYVERTEX;
    float r = static_cast<float>( GetColour().Red() ) / 255.0f;
    float g = static_cast<float>( GetColour().Green() ) / 255.0f;
    float b = static_cast<float>( GetColour().Blue() ) / 255.0f;
    float a = GetAlpha();
    for( ; i < end; i++ )
    {
        mDisplayVertexColour[i].Set(
            static_cast<int>( static_cast<float>( mVertexColour[i].Red() ) * r ), 
            static_cast<int>( static_cast<float>( mVertexColour[i].Green() ) * g ), 
            static_cast<int>( static_cast<float>( mVertexColour[i].Blue() ) * b ), 
            static_cast<int>( static_cast<float>( mVertexColour[i].Alpha() ) * a ) 
            );
    }
}

void FePolygon::SetAlpha( float a )
{
    FeDrawable::SetAlpha( a );
//    RecalculateDisplayColour();
    for( int i = 0; i < mNumVertex; i++ )
    {
        mDisplayVertexColour[ i ].SetAlpha( static_cast<int>( a * 255.0f ) );
    }
}

void FePolygon::SetColour( tColour c )
{
    FeDrawable::SetColour( c );
    RecalculateDisplayColour();
}


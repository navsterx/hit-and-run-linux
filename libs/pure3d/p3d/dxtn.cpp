/*===========================================================================
    bmp.cpp
    15-Feb-00 Created by Neall

    Copyright (c) 2000 Radical Entertainment, Inc.
    All rights reserved.
===========================================================================*/

#include <p3d/image.hpp>
#include <p3d/dxtn.hpp>
#include <p3d/file.hpp>
#include <p3d/error.hpp>
#include <p3d/utility.hpp>
#include <pddi/pdditype.hpp>
#include <string.h>



//////////////////////////////////////////////////////////////////////////////////////
///     all data struct defined were copied from ddraw.h to avoid of
///     including ddraw.h which is dependent on PC only

/*
 * dwLinearSize is valid
 */
#define DDSD_LINEARSIZE         0x00080000l


#ifndef DUMMYUNIONNAMEN
#if defined(__cplusplus) || !defined(NONAMELESSUNION)
#define DUMMYUNIONNAMEN(n)
#else
#define DUMMYUNIONNAMEN(n)      u##n
#endif
#endif


#ifndef MAKEFOURCC
     #define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                     ((unsigned int)(unsigned char)(ch0) | ((unsigned int)(unsigned char)(ch1) << 8) |   \
                     ((unsigned int)(unsigned char)(ch2) << 16) | ((unsigned int)(unsigned char)(ch3) << 24 ))
#endif //defined(MAKEFOURCC)                     

/*
 * DDCOLORKEY
 */
typedef struct _DDCOLORKEY
{
     unsigned int       dwColorSpaceLowValue;   // low boundary of color space that is to
                                                     // be treated as Color Key, inclusive
     unsigned int       dwColorSpaceHighValue;  // high boundary of color space that is
                                                     // to be treated as Color Key, inclusive
} DDCOLORKEY;


/*
 * DDSCAPS2
 */
typedef struct _DDSCAPS2
{
     unsigned int       dwCaps;         // capabilities of surface wanted
     unsigned int       dwCaps2;
     unsigned int       dwCaps3;
     union
     {
          unsigned int       dwCaps4;
          unsigned int       dwVolumeDepth;
     } DUMMYUNIONNAMEN(1);
} DDSCAPS2;


/*
 * DDPIXELFORMAT
 */
typedef struct _DDPIXELFORMAT
{
     unsigned int       dwSize;                 // size of structure
     unsigned int       dwFlags;                // pixel format flags
     unsigned int       dwFourCC;               // (FOURCC code)
     union
     {
          unsigned int   dwRGBBitCount;          // how many bits per pixel
          unsigned int   dwYUVBitCount;          // how many bits per pixel
          unsigned int   dwZBufferBitDepth;      // how many total bits/pixel in z buffer (including any stencil bits)
          unsigned int   dwAlphaBitDepth;        // how many bits for alpha channels
          unsigned int   dwLuminanceBitCount;    // how many bits per pixel
          unsigned int   dwBumpBitCount;         // how many bits per "buxel", total
          unsigned int   dwPrivateFormatBitCount;// Bits per pixel of private driver formats. Only valid in texture
                                                     // format list and if DDPF_D3DFORMAT is set
     } DUMMYUNIONNAMEN(1);
     union
     {
          unsigned int   dwRBitMask;             // mask for red bit
          unsigned int   dwYBitMask;             // mask for Y bits
          unsigned int   dwStencilBitDepth;      // how many stencil bits (note: dwZBufferBitDepth-dwStencilBitDepth is total Z-only bits)
          unsigned int   dwLuminanceBitMask;     // mask for luminance bits
          unsigned int   dwBumpDuBitMask;        // mask for bump map U delta bits
          unsigned int   dwOperations;           // DDPF_D3DFORMAT Operations
     } DUMMYUNIONNAMEN(2);
     union
     {
          unsigned int   dwGBitMask;             // mask for green bits
          unsigned int   dwUBitMask;             // mask for U bits
          unsigned int   dwZBitMask;             // mask for Z bits
          unsigned int   dwBumpDvBitMask;        // mask for bump map V delta bits
          struct
          {
                unsigned short    wFlipMSTypes;       // Multisample methods supported via flip for this D3DFORMAT
                unsigned short    wBltMSTypes;        // Multisample methods supported via blt for this D3DFORMAT
          } MultiSampleCaps;

     } DUMMYUNIONNAMEN(3);
     union
     {
          unsigned int   dwBBitMask;             // mask for blue bits
          unsigned int   dwVBitMask;             // mask for V bits
          unsigned int   dwStencilBitMask;       // mask for stencil bits
          unsigned int   dwBumpLuminanceBitMask; // mask for luminance in bump map
     } DUMMYUNIONNAMEN(4);
     union
     {
          unsigned int   dwRGBAlphaBitMask;      // mask for alpha channel
          unsigned int   dwYUVAlphaBitMask;      // mask for alpha channel
          unsigned int   dwLuminanceAlphaBitMask;// mask for alpha channel
          unsigned int   dwRGBZBitMask;          // mask for Z channel
          unsigned int   dwYUVZBitMask;          // mask for Z channel
     } DUMMYUNIONNAMEN(5);
} DDPIXELFORMAT;

/*
 * DDSURFACEDESC2
 */
typedef struct _DDSURFACEDESC2
{
     unsigned int               dwSize;                 // size of the DDSURFACEDESC structure
     unsigned int               dwFlags;                // determines what fields are valid
     unsigned int               dwHeight;               // height of surface to be created
     unsigned int               dwWidth;                // width of input surface
     union
     {
          int            lPitch;                 // distance to start of next line (return value only)
          unsigned int           dwLinearSize;           // Formless late-allocated optimized surface size
     } DUMMYUNIONNAMEN(1);
     union
     {
          unsigned int           dwBackBufferCount;      // number of back buffers requested
          unsigned int           dwDepth;                // the depth if this is a volume texture 
     } DUMMYUNIONNAMEN(5);
     union
     {
          unsigned int           dwMipMapCount;          // number of mip-map levels requestde
                                                                // dwZBufferBitDepth removed, use ddpfPixelFormat one instead
          unsigned int           dwRefreshRate;          // refresh rate (used when display mode is described)
          unsigned int           dwSrcVBHandle;          // The source used in VB::Optimize
     } DUMMYUNIONNAMEN(2);
     unsigned int               dwAlphaBitDepth;        // depth of alpha buffer requested
     unsigned int               dwReserved;             // reserved
     void *              lpSurface;              // pointer to the associated surface memory
     union
     {
          DDCOLORKEY      ddckCKDestOverlay;      // color key for destination overlay use
          unsigned int           dwEmptyFaceColor;       // Physical color for empty cubemap faces
     } DUMMYUNIONNAMEN(3);
     DDCOLORKEY          ddckCKDestBlt;          // color key for destination blt use
     DDCOLORKEY          ddckCKSrcOverlay;       // color key for source overlay use
     DDCOLORKEY          ddckCKSrcBlt;           // color key for source blt use
     union
     {
          DDPIXELFORMAT   ddpfPixelFormat;        // pixel format description of the surface
          unsigned int           dwFVF;                  // vertex format description of vertex buffers
     } DUMMYUNIONNAMEN(4);
     DDSCAPS2            ddsCaps;                // direct draw surface capabilities
     unsigned int               dwTextureStage;         // stage in multitexture cascade
} DDSURFACEDESC2;





static void LoadDXTNFile(tFile*,  tImageHandler::Builder* );
static pddiPixelFormat DecodePixelFormat(  DDPIXELFORMAT*  );


bool tDXTNHandler::CheckFormat(Format format)
{
    switch(format)
    {
        case IMG_DXT:
        case IMG_DXT1:
        case IMG_DXT2:            
        case IMG_DXT3:            
        case IMG_DXT4:            
        case IMG_DXT5:
            return true;
    }
    return false;
}

void tDXTNHandler::CreateImage(tFile* file, tImageHandler::Builder* builder)
{
#ifdef RAD_GAMECUBE
    bool origswap = file->GetEndianSwap();
    file->SetEndianSwap(true);
#endif
    
    LoadDXTNFile( file, builder );
#ifdef RAD_GAMECUBE
    file->SetEndianSwap(origswap);
#endif
}

bool tDXTNHandler::SaveImage(tImage*, char*)
{
    return false;
}

#ifdef RAD_LINUX
//-------------------------------------------------------------------
// Software DXT1/3/5 decoder (DXT2/4 are the premultiplied variants and decode like 3/5).
// Output pixels are 0xAARRGGBB, the layout ProcessScanline32 expects (as in png.cpp).
static unsigned Expand565( unsigned short c )
{
    unsigned r = ( c >> 11 ) & 31, g = ( c >> 5 ) & 63, b = c & 31;
    return ( ( r << 3 | r >> 2 ) << 16 ) | ( ( g << 2 | g >> 4 ) << 8 ) | ( b << 3 | b >> 2 );
}

static unsigned Lerp( unsigned a, unsigned b, int wa, int wb, int div )
{
    unsigned out = 0;
    for( int shift = 0; shift <= 16; shift += 8 )
    {
        unsigned ca = ( a >> shift ) & 255, cb = ( b >> shift ) & 255;
        out |= ( ( ca * wa + cb * wb ) / div ) << shift;
    }
    return out;
}

// colour block (8 bytes) -> 16 RGB values + per-pixel "transparent" flag (DXT1 3-colour mode)
static void DecodeColourBlock( const unsigned char* blk, bool dxt1, unsigned rgb[ 16 ], bool transparent[ 16 ] )
{
    unsigned short c0 = blk[ 0 ] | ( blk[ 1 ] << 8 ), c1 = blk[ 2 ] | ( blk[ 3 ] << 8 );
    unsigned pal[ 4 ];
    pal[ 0 ] = Expand565( c0 );
    pal[ 1 ] = Expand565( c1 );
    bool fourColour = !dxt1 || c0 > c1;
    if( fourColour )
    {
        pal[ 2 ] = Lerp( pal[ 0 ], pal[ 1 ], 2, 1, 3 );
        pal[ 3 ] = Lerp( pal[ 0 ], pal[ 1 ], 1, 2, 3 );
    }
    else
    {
        pal[ 2 ] = Lerp( pal[ 0 ], pal[ 1 ], 1, 1, 2 );
        pal[ 3 ] = 0;
    }
    unsigned bits = blk[ 4 ] | ( blk[ 5 ] << 8 ) | ( blk[ 6 ] << 16 ) | ( (unsigned) blk[ 7 ] << 24 );
    for( int i = 0; i < 16; i++ )
    {
        unsigned idx = ( bits >> ( 2 * i ) ) & 3;
        rgb[ i ] = pal[ idx ];
        transparent[ i ] = !fourColour && idx == 3;
    }
}

static void LoadDXTNAsRGBA( tFile* file, tImageHandler::Builder* builder, int width, int height, pddiTextureType type )
{
    bool dxt1 = ( type == PDDI_TEXTYPE_DXT1 );
    bool dxt5 = ( type == PDDI_TEXTYPE_DXT5 || type == PDDI_TEXTYPE_DXT4 );
    int blockBytes = dxt1 ? 8 : 16;
    int bw = ( width + 3 ) / 4, bh = ( height + 3 ) / 4;
    int compressedSize = bw * bh * blockBytes;

    unsigned char* compressed = (unsigned char*) p3d::MallocTemp( compressedSize );
    unsigned* pixels = (unsigned*) p3d::MallocTemp( width * height * 4 );
    if( compressed == NULL || pixels == NULL ) return;
    file->GetData( compressed, compressedSize, tFile::BYTE );

    for( int by = 0; by < bh; by++ )
    {
        for( int bx = 0; bx < bw; bx++ )
        {
            const unsigned char* blk = compressed + ( by * bw + bx ) * blockBytes;
            unsigned alpha[ 16 ];
            if( dxt1 )
            {
                for( int i = 0; i < 16; i++ ) alpha[ i ] = 255;
            }
            else if( dxt5 )
            {
                unsigned a0 = blk[ 0 ], a1 = blk[ 1 ], apal[ 8 ];
                apal[ 0 ] = a0; apal[ 1 ] = a1;
                if( a0 > a1 )
                    for( int k = 1; k < 7; k++ ) apal[ k + 1 ] = ( a0 * ( 7 - k ) + a1 * k ) / 7;
                else
                {
                    for( int k = 1; k < 5; k++ ) apal[ k + 1 ] = ( a0 * ( 5 - k ) + a1 * k ) / 5;
                    apal[ 6 ] = 0;
                    apal[ 7 ] = 255;
                }
                unsigned long long abits = 0;
                for( int k = 0; k < 6; k++ ) abits |= (unsigned long long) blk[ 2 + k ] << ( 8 * k );
                for( int i = 0; i < 16; i++ ) alpha[ i ] = apal[ ( abits >> ( 3 * i ) ) & 7 ];
            }
            else    // DXT2/3: explicit 4-bit alpha
            {
                for( int i = 0; i < 16; i++ )
                {
                    unsigned a = ( blk[ i / 2 ] >> ( ( i & 1 ) * 4 ) ) & 15;
                    alpha[ i ] = a << 4 | a;
                }
            }

            unsigned rgb[ 16 ];
            bool transparent[ 16 ];
            DecodeColourBlock( blk + ( dxt1 ? 0 : 8 ), dxt1, rgb, transparent );

            for( int i = 0; i < 16; i++ )
            {
                int x = bx * 4 + ( i & 3 ), y = by * 4 + ( i >> 2 );
                if( x < width && y < height )
                    pixels[ y * width + x ] = ( transparent[ i ] ? 0 : ( alpha[ i ] << 24 ) ) | rgb[ i ];
            }
        }
    }

    // D3D8 copied DXT blocks into the texture as-is (FillLockInfo skips its usual bottom-up row flip for DXT
    // formats), so the art stores DXT images bottom row first. Deliver rows in file order: BOTTOM undoes the
    // row flip that every other texture gets through Lock().
    if( builder->BeginImage( width, height, 32, tImageHandler::Builder::BOTTOM, NULL ) )
    {
        for( int y = 0; y < height; y++ )
            builder->ProcessScanline32( pixels + y * width );
        builder->EndImage( );
    }
    p3d::FreeTemp( pixels );
    p3d::FreeTemp( compressed );
}
#endif

static void LoadDXTNFile(tFile* file,  tImageHandler::Builder* builder)
{
/////////////////////////////////////////
///     ProcessScanline8 need to be rewritten to for DXTN format, or May be
////    we should call another new func to add the read-in data to texture
///  
     // get palette
     long ids;

     //read magic number
     file->GetData(&ids, 1, tFile::DWORD);

     if (ids == MAKEFOURCC(' ','S','D','D'))
     {
          file->SetEndianSwap(!file->GetEndianSwap());
     }

     DDSURFACEDESC2 header;

     file->GetData( (void*) &header, sizeof( DDSURFACEDESC2 ) / 4, tFile::DWORD );

     //set the  right texture type for the DXTn format
     pddiPixelFormat format = DecodePixelFormat( &header.ddpfPixelFormat );
     pddiTextureType type;
     switch( format )
     {
     case PDDI_PIXEL_DXT1:
          type = PDDI_TEXTYPE_DXT1;
          break;

     case PDDI_PIXEL_DXT2:
          type = PDDI_TEXTYPE_DXT2;
          break;

     case PDDI_PIXEL_DXT3:
          type = PDDI_TEXTYPE_DXT3;
          break;
        
     case PDDI_PIXEL_DXT4:
          type = PDDI_TEXTYPE_DXT4;
          break;

     case PDDI_PIXEL_DXT5:
          type = PDDI_TEXTYPE_DXT5;
          break;

     default:    //error;
          return;
     }

#ifdef RAD_LINUX
     // The OpenGL PDDI has no compressed texture formats: decompress to 32-bit ARGB rows instead.
     LoadDXTNAsRGBA( file, builder, header.dwWidth, header.dwHeight, type );
     return;
#endif

     builder->SetTextureType( type );

     bool ok = builder->BeginImage(header.dwWidth, header.dwHeight, 2, tImageHandler::Builder::TOP, NULL );
     if (!ok) return; // Can't load on this platform


//    file->SetPosition( sizeof(long) + sizeof(DDSURFACEDESC2) );

     if( header.dwFlags & DDSD_LINEARSIZE )
     {
          unsigned char *scanline_in = (unsigned char*) builder->GetMemoryImagePtr();
          if (scanline_in == NULL) return;
          file->GetData(scanline_in, header.dwLinearSize, tFile::BYTE);
     }
     else
     {
          int width = header.dwWidth * header.ddpfPixelFormat.dwRGBBitCount / 8;

          //each time we read int data of "width" long, although the
          //real data space take "lPitch" long.
          unsigned char * scanline_in = (unsigned char*)p3d::MallocTemp(header.lPitch*header.dwHeight);

          if( scanline_in == NULL ) return;

          unsigned char *buf = scanline_in;

          for( unsigned int yp = 0; yp < header.dwHeight; yp++ )
          {
                file->GetData(buf, width, tFile::BYTE);
                buf += header.lPitch;            
          }

          builder->DirectCopy(scanline_in, header.lPitch*header.dwHeight );
          p3d::FreeTemp(scanline_in);

     }
     builder->EndImage();
}

static pddiPixelFormat DecodePixelFormat(  DDPIXELFORMAT* pddpf )
{
     switch( pddpf->dwFourCC )
     {

          case MAKEFOURCC('D','X','T','1'):
                return PDDI_PIXEL_DXT1;           

          case MAKEFOURCC('D','X','T','2'):
                return PDDI_PIXEL_DXT2;  

          case MAKEFOURCC('D','X','T','3'):
                return PDDI_PIXEL_DXT3;      

          case MAKEFOURCC('D','X','T','4'):
                return PDDI_PIXEL_DXT4;  

          case MAKEFOURCC('D','X','T','5'):
                return PDDI_PIXEL_DXT5;  
             
        default:
                return PDDI_PIXEL_UNKNOWN;
     }
}


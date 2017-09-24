#include <windows.h>
#include <gdiplus.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>

using namespace Gdiplus;

static const int LumaRed = 9798;	//	0.299
static const int LumaGreen = 19235;	//	0.587
static const int LumaBlue = 3735;	//	0.114
static const int CoeffNormalizationBitsCount = 15;
static const int CoeffNormalization = 1 << CoeffNormalizationBitsCount;

struct pixel
{
	int R;
	int G;
	int B;
};

pixel GetPixel()
{
	return pixel();
}

class CImage {
public:
	const int w;
	const int h;
	const int bpr;
	const int bpp;
	BYTE *pBuffer;
	
	CImage( BitmapData& pData ) : 
			w( pData.Width ), 
			h( pData.Height ),
			bpr( pData.Stride ),
			bpp( 3 ), 
			pBuffer( ( BYTE * )pData.Scan0 ) {
	}
	
	

	void GrayscaleDemosacing( int top = 0, bot = h, )
	{
		int top = 1324; // 0;
		int bot = 1536; // h;
		int left = 1720; // 0;
		int right = 2462; // w;

		int baseAdr = bpr*top + bpp*left; // 0;
		for( int y = top; y < bot; y++ ) {
			int pixelAdr = baseAdr;
			for( int x = left; x < right; x++ ) {
				//// красный пиксель
				//if( x % 2 == 0 && y % 2 == 0 ) {
				//	// printf( "%d %d \n", x, y );
				//	int R = pBuffer[pixelAdr + 2]; // red
				//	continue;
				//}
				//// синий пиксель
				//if( x % 2 == 1 && y % 2 == 1 ) {
				//	// printf( "%d %d \n", x, y );
				//	continue;
				//}
				// printf( "%d %d \n", x, y );
				// замечание, для исходной картинки эти три значения всегда равны
				int B = pBuffer[pixelAdr]; // blue
				int G = pBuffer[pixelAdr + 1]; // green
				int R = pBuffer[pixelAdr + 2]; // red

				int Y = ( LumaRed * R + LumaGreen * G + LumaBlue * B + ( CoeffNormalization >> 1 ) )
					>> CoeffNormalizationBitsCount; // luminance
													
				// desaturation by 50%
				// no need to check for the interval [0..255]
				pBuffer[pixelAdr] = ( Y + B ) >> 1;
				pBuffer[pixelAdr + 1] = 255; // ( Y + G ) >> 1;
				pBuffer[pixelAdr + 2] = ( Y + R ) >> 1;

				pixelAdr += bpp;
			}
			baseAdr += bpr;
		}
	}
};

void demosacing( BitmapData& pData )
{
	const int w = pData.Width;
	const int h = pData.Height;
	const int bpr = pData.Stride;
	const int bpp = 3; // BGR24
	BYTE *pBuffer = ( BYTE * )pData.Scan0;
	CImage image( pData );

	time_t start = clock();

	image.GrayscaleDemosacing();

	time_t end = clock();
	_tprintf( _T( "Time: %.3f\n" ), static_cast<double>( end - start ) / CLOCKS_PER_SEC );
}

void grayscaleDemosacing( BitmapData& pData )
{
	const int w = pData.Width;
	const int h = pData.Height;
	const int bpr = pData.Stride;
	const int bpp = 3; // BGR24
	BYTE *pBuffer = ( BYTE * )pData.Scan0;

	time_t start = clock();

	int baseAdr = 0;
	for( int y = 0; y < h; y++ ) {
		int pixelAdr = baseAdr;
		for( int x = 0; x < w; x++ ) {
			int B = pBuffer[pixelAdr]; // blue
			int G = pBuffer[pixelAdr + 1]; // green
			int R = pBuffer[pixelAdr + 2]; // red

			int Y = ( LumaRed * R + LumaGreen * G + LumaBlue * B + ( CoeffNormalization >> 1 ) )
				>> CoeffNormalizationBitsCount; // luminance

												// desaturation by 50%
												// no need to check for the interval [0..255]
			pBuffer[pixelAdr] = ( Y + B ) >> 1;
			pBuffer[pixelAdr + 1] = ( Y + G ) >> 1;
			pBuffer[pixelAdr + 2] = ( Y + R ) >> 1;

			pixelAdr += bpp;
		}
		baseAdr += bpr;
	}

	time_t end = clock();
	_tprintf( _T( "Time: %.3f\n" ), static_cast<double>( end - start ) / CLOCKS_PER_SEC );
}

void process( BitmapData& pData )
{
	// grayscaleDemosacing( pData );
	demosacing( pData );
}

int GetEncoderClsid( const WCHAR* format, CLSID* pClsid )
{
   UINT  num = 0;          // number of image encoders
   UINT  size = 0;         // size of the image encoder array in bytes

   ImageCodecInfo* pImageCodecInfo = NULL;

   GetImageEncodersSize( &num, &size );
   if( size == 0 )
      return -1;  // Failure

   pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
   if( pImageCodecInfo == NULL )
      return -1;  // Failure

   GetImageEncoders( num, size, pImageCodecInfo );

   for( UINT j = 0; j < num; j++ )
   {
      if( wcscmp( pImageCodecInfo[j].MimeType, format ) == 0 )
      {
         *pClsid = pImageCodecInfo[j].Clsid;
         free(pImageCodecInfo);
         return j;  // Success
      }    
   }

   free( pImageCodecInfo );
   return -1;  // Failure
}

int _tmain(int argc, _TCHAR* argv[])
{
	if( argc != 3 )
	{
		_tprintf( _T("Usage: BayerPattern <inputFile.bmp> <outputFile.bmp>\n") );
		return 0;
	}

	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR			gdiplusToken;
	GdiplusStartup( &gdiplusToken, &gdiplusStartupInput, NULL );

	{	
		Bitmap GDIBitmap( argv[1] );

		int w = GDIBitmap.GetWidth();
		int h = GDIBitmap.GetHeight();
		BitmapData bmpData;
		Rect rc( 0, 0, w, h ); // whole image
		if( Ok != GDIBitmap.LockBits( &rc, ImageLockModeRead | ImageLockModeWrite, PixelFormat24bppRGB, &bmpData ) )
			_tprintf( _T("Failed to lock image: %s\n"), argv[1] );
		else
			_tprintf( _T("File: %s\n"), argv[1] );

		process( bmpData );

		GDIBitmap.UnlockBits( &bmpData );

		// Save result
		CLSID clsId;
		GetEncoderClsid( _T("image/bmp"), &clsId );
		GDIBitmap.Save( argv[2], &clsId, NULL );
	}

	GdiplusShutdown( gdiplusToken );
	system( "pause" );
	return 0;
}
#include <windows.h>
#include <gdiplus.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>
#include <assert.h>

using namespace Gdiplus;

static const int LumaRed = 9798;	//	0.299
static const int LumaGreen = 19235;	//	0.587
static const int LumaBlue = 3735;	//	0.114
static const int CoeffNormalizationBitsCount = 15;
static const int CoeffNormalization = 1 << CoeffNormalizationBitsCount;

struct Pixel
{
	int R;
	int G;
	int B;
	Pixel( int R, int G, int B ) : R( R ), G( G ), B( B ) {};
};

struct Bound{
	int top;
	int bot;
	int left;
	int right;
	Bound() : 
			top( 0 ), bot( 0 ), left( 0 ), right( 0 ) {}
	Bound( int top, int bot, int left, int right ) : 
			top( top ), bot( bot ), left( left ), right( right ) {}
};

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
	
	int CheckTop( int top )
	{
		if( top < 0 ) {
			return 0;
		}
		if( top > h ) {
			return h;
		}
		return top;
	}
	int CheckBot( int bot )
	{
		if( bot < 0 ) {
			return 0;
		}
		if( bot == 0 || bot > h ) {
			return h;
		}
		return bot;
	}
	int CheckLeft( int left )
	{
		if( left < 0 ) {
			return 0;
		}
		if( left > w ) {
			return w;
		}
		return left;
	}
	int CheckRight( int right )
	{
		if( right < 0 ) {
			return 0;
		}
		if( right == 0 || right > w ) {
			return w;
		}
		return right;
	}
	// возвращает границы для безопасного обращения к куску картинки
	Bound CheckBound( Bound& bound )
	{
		bound.top = CheckTop( bound.top );
		bound.bot = CheckBot( bound.bot );
		assert( bound.top <= bound.bot );
		bound.left = CheckLeft( bound.left );
		bound.right = CheckRight( bound.right );
		assert( bound.left <= bound.right );
		return bound;
	}

	int CheckX( int x )
	{
		if( x < 0 ) {
			return 0;
		}
		if( x > w ) {
			return w;
		}
		return x;
	}
	int CheckY( int y )
	{
		if( y < 0 ) {
			return 0;
		}
		if( y > h ) {
			return h;
		}
		return y;
	}
	// возвращает пиксель, делая проверки на безопасное обращение
	// а также учитывающие stride и прочее
	Pixel GetPixel( int x, int y )
	{
		x = CheckX( x );
		y = CheckY( y );
		int adr = bpr*y + bpp*x;
		return Pixel( pBuffer[adr + 2], pBuffer[adr + 1], pBuffer[adr] );
	}

	void GrayscaleDemosacing( int top = 0, int bot = 0, int left = 0, int right = 0 )
	{
		//Bound bound( top, bot, left, right );
		//CheckBound( bound );
		Bound bound( CheckBound( Bound( top, bot, left, right ) ) );
		top = bound.top;
		bot = bound.bot;
		left = bound.left;
		right = bound.right;

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

				Pixel pixel = GetPixel( x, y );
				int b = pixel.B;
				int g = pixel.G;
				int r = pixel.R;

				assert( B == b );
				assert( G == g );
				assert( R == r );

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
	//const int w = pData.Width;
	//const int h = pData.Height;
	//const int bpr = pData.Stride;
	//const int bpp = 3; // BGR24
	//BYTE *pBuffer = ( BYTE * )pData.Scan0;
	CImage image( pData );

	time_t start = clock();
	// image.GrayscaleDemosacing( 1324, 1536, 1720, 2462 );
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
		system( "pause" );
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
	system( "openProcImage.cmd" );
	// system( "pause" );
	return 0;
}
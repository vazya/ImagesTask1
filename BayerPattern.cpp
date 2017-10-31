#include <windows.h>
#include <gdiplus.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>
#include <assert.h>
#include <cmath>

using namespace Gdiplus;

static const float EPS = 0.01;

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
	// возвращает яркость
	float GetY()
	{
		return ((LumaRed * R + LumaGreen * G + LumaBlue * B + (CoeffNormalization >> 1)) >> CoeffNormalizationBitsCount);
	}
};

struct Bound{
	int top;
	int bot;
	int left;
	int right;
	Bound() : top( 0 ), bot( 0 ), left( 0 ), right( 0 ) {}
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
		pBuffer( (BYTE *)pData.Scan0 )
	{
	}

	int GetAdr( int x, int y )
	{
		return bpr*y + bpp*x;
	}

	int CheckTop( int top )
	{
		return top == 0 ? 0 : max( 0, min( top, h ) );
	}
	int CheckBot( int bot )
	{
		return bot == 0 ? h : max( 0, min( bot, h ) );
	}
	int CheckLeft( int left )
	{
		return left == 0 ? 0 : max( 0, min( left, w ) );
	}
	int CheckRight( int right )
	{
		return right == 0 ? w : max( 0, min( right, w ) );
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
		return max( 0, min( x, w - 1 ) );
	}
	int CheckY( int y )
	{
		return max( 0, min( y, h - 1 ) );
	}
	// возвращает пиксель из исходного изображения, делая безопасное обращение
	// а также учитывающие stride и прочее
	Pixel GetPixel( int x, int y )
	{
		int adr = GetAdr( CheckX( x ), CheckY( y ) );
		return Pixel( pBuffer[adr + 2], pBuffer[adr + 1], pBuffer[adr] );
	}

	void SetPixel( int x, int y, int R, int G, int B )
	{
		int adr = GetAdr( CheckX( x ), CheckY( y ) );
		pBuffer[adr] = CheckValue( B );
		pBuffer[adr + 1] = CheckValue( G );
		pBuffer[adr + 2] = CheckValue( R );
	}
	// возвращает индекс наименьшего элемента, т.е. если min == с вернет 2
	int Min( int a, int b, int c, int d )
	{
		int v[4] = { a, b, c, d };
		int i = (v[0] < v[1]) ? 0 : 1;
		int j = (v[2] < v[3]) ? 2 : 3;
		return (v[i] < v[j]) ? i : j;
	}

	int CheckValue( int value )
	{
		return max( 0, min( value, 255 ) );
	}

	int HueTransit( int l1, int l2, int l3, int v1, int v3 )
	{
		if( ( l1 < l2 && l2 < l3 ) || ( l1 > l2 && l2 > l3 ) ) {
			return v1 + ( v3 - v1 )*( l2 - l1 ) / ( l3 - l1 );
		} else {
			return ( v1 + v3 ) / 2 + ( l2 - ( l1 + l3 ) / 2 ) / 2 ;
		}
	}

	float Compare( CImage &original )
	{
		assert( w == original.w );
		assert( h == original.h );
		// mean square error
		float mse = 0;
		for( int y = 0; y < h; y++ ) {
			for( int x = 0; x < w; x++ ) {
				float Y = GetPixel( x, y ).GetY();
				float originalY = original.GetPixel( x, y ).GetY();
				mse += pow( abs( Y - originalY ), 2 );
			}
		}
		mse /= (h*w);
		_tprintf( _T( "mse: %f\n" ), mse );
		// непосредственно сама метрика PSNR
		return 10 * log10f( float( 255*255 ) / ( mse + 0.0000001 ) );
	}

	// Patterned Pixel Grouping - усреднение по ближайшим соседям-пикселям искомого цвета
	void PPG( int top = 0, int bot = 0, int left = 0, int right = 0 )
	{
		Bound bound( CheckBound( Bound( top, bot, left, right ) ) );
		top = bound.top;
		bot = bound.bot;
		left = bound.left;
		right = bound.right;
		// восстановление G в R или B пикселях
		for( int y = top; y < bot; y++ ) {
			for( int x = left; x < right; x++ ) {
				if( x == 1759 && y == 1367 ) {
					Pixel p2 = GetPixel( x - 1, y - 1 );
					Pixel p3 = GetPixel( x, y - 1 );
					Pixel p4 = GetPixel( x + 1, y - 1 );
					Pixel p7 = GetPixel( x - 1, y );
					Pixel p8 = GetPixel( x, y );
					Pixel p9 = GetPixel( x + 1, y );
					Pixel p12 = GetPixel( x - 1, y + 1 );
					Pixel p13 = GetPixel( x, y + 1 );
					Pixel p14 = GetPixel( x + 1, y + 1 );
					int u = 0;
				}
				Pixel pixel = GetPixel( x, y );
				int R = pixel.R;
				int G = pixel.G;
				int B = pixel.B;
				// красный пиксель - для этих пикселей был красный детектор интенсивности
				if( x % 2 == 0 && y % 2 == 0 ) {
					int R13 = R;
					int R3 = GetPixel( x, y - 2 ).R;
					int R15 = GetPixel( x + 2, y ).R;
					int R11 = GetPixel( x - 2, y ).R;
					int R23 = GetPixel( x, y + 2 ).R;

					int G8 = GetPixel( x, y - 1 ).G;
					int G18 = GetPixel( x, y + 1 ).G;
					int G12 = GetPixel( x - 1, y ).G;
					int G14 = GetPixel( x + 1, y ).G;

					int deltaN = abs( R13 - R3 ) * 2 + abs( G8 - G18 );
					int deltaE = abs( R13 - R15 ) * 2 + abs( G12 - G14 );
					int deltaW = abs( R13 - R11 ) * 2 + abs( G12 - G14 );
					int deltaS = abs( R13 - R23 ) * 2 + abs( G8 - G18 );

					switch( Min( deltaN, deltaE, deltaW, deltaS ) ) {
						case 0: {
							G = (G8 * 3 + G18 + R13 - R3) / 4;
							break;
						}
						case 1: {
							G = (G14 * 3 + G12 + R13 - R15) / 4;
							break;
						}
						case 2: {
							G = (G12 * 3 + G14 + R13 - R11) / 4;
							break;
						}
						case 3: {
							G = (G18 * 3 + G8 + R13 - R23) / 4;
							break;
						}
						default:
							assert( false );
					}
				}
				// синий пиксель
				if( x % 2 == 1 && y % 2 == 1 ) {
					int B13 = B;
					int B3 = GetPixel( x, y - 2 ).B;
					int B15 = GetPixel( x + 2, y ).B;
					int B11 = GetPixel( x - 2, y ).B;
					int B23 = GetPixel( x, y + 2 ).B;

					int G8 = GetPixel( x, y - 1 ).G;
					int G18 = GetPixel( x, y + 1 ).G;
					int G12 = GetPixel( x - 1, y ).G;
					int G14 = GetPixel( x + 1, y ).G;

					int deltaN = abs( B13 - B3 ) * 2 + abs( G8 - G18 );
					int deltaE = abs( B13 - B15 ) * 2 + abs( G12 - G14 );
					int deltaW = abs( B13 - B11 ) * 2 + abs( G12 - G14 );
					int deltaS = abs( B13 - B23 ) * 2 + abs( G8 - G18 );

					switch( Min( deltaN, deltaE, deltaW, deltaS ) ) {
						case 0: {
							G = int( float(G8 * 3 + G18 + B13 - B3) / 4 );
							break;
						}
						case 1: {
							G = int( float(G14 * 3 + G12 + B13 - B15) / 4);
							break;
						}
						case 2: {
							G = int( float(G12 * 3 + G14 + B13 - B11) / 4);
							break;
						}
						case 3: {
							G = int( float(G18 * 3 + G8 + B13 - B23) / 4);
							break;
						}
						default:
							assert( false );
					}
				}

				SetPixel( x, y, R, G, B );
			}
		}
		// восстановление R и B в G пикселях
		for( int y = top; y < bot; y++ ) {
			for( int x = left; x < right; x++ ) {
				Pixel pixel = GetPixel( x, y );
				int B = pixel.B;
				int G = pixel.G;
				int R = pixel.R;
				// зеленый пиксель
				if( x % 2 == 0 && y % 2 == 1 ) {
					int G8 = G;
					int G7 = GetPixel( x - 1, y ).G;
					int G9 = GetPixel( x + 1, y ).G;
					int B7 = GetPixel( x - 1, y ).B;
					int B9 = GetPixel( x + 1, y ).B;
					B = HueTransit( G7, G8, G9, B7, B9 );

					int G3 = GetPixel( x, y - 1 ).G;
					int G13 = GetPixel( x, y + 1 ).G;
					int R3 = GetPixel( x, y - 1 ).R;
					int R13 = GetPixel( x, y + 1 ).R;
					R = HueTransit( G3, G8, G13, R3, R13 );
				}
				if( x % 2 == 1 && y % 2 == 0 ) {
					int G8 = G;
					int G7 = GetPixel( x - 1, y ).G;
					int G9 = GetPixel( x + 1, y ).G;
					int R7 = GetPixel( x - 1, y ).R;
					int R9 = GetPixel( x + 1, y ).R;
					R = HueTransit( G7, G8, G9, R7, R9 );

					int G3 = GetPixel( x, y - 1 ).G;
					int G13 = GetPixel( x, y + 1 ).G;
					int B3 = GetPixel( x, y - 1 ).B;
					int B13 = GetPixel( x, y + 1 ).B;
					B = HueTransit( G3, G8, G13, B3, B13 );
				}
				SetPixel( x, y, R, G, B );
			}
		}
		// восстановление R и B в B и R пикселях
		for( int y = top; y < bot; y++ ) {
			for( int x = left; x < right; x++ ) {
				Pixel pixel = GetPixel( x, y );
				int R = pixel.R;
				int G = pixel.G;
				int B = pixel.B;
				// красный пиксель - для этих пикселей был красный детектор интенсивности
				if( x % 2 == 0 && y % 2 == 0 ) {
					int R13 = R;
					int R1 = GetPixel( x - 2, y - 2 ).R;
					int R5 = GetPixel( x + 2, y - 2 ).R;
					int R21 = GetPixel( x - 2, y + 2 ).R;
					int R25 = GetPixel( x + 2, y + 2 ).R;

					int B7 = GetPixel( x - 1, y - 1 ).B;
					int B9 = GetPixel( x + 1, y - 1 ).B;
					int B17 = GetPixel( x - 1, y + 1 ).B;
					int B19 = GetPixel( x + 1, y + 1 ).B;

					int G13 = G;
					int G7 = GetPixel( x - 1, y - 1 ).G;
					int G9 = GetPixel( x + 1, y - 1 ).G;
					int G17 = GetPixel( x - 1, y + 1 ).G;
					int G19 = GetPixel( x + 1, y + 1 ).G;

					int deltaNE = abs( B9 - B17 ) + abs( R5 - R13 ) + abs( R13 - R21 ) 
							+ abs( G9 - G13 ) + abs( G13 - G17 );
					int deltaNW = abs( B7 - B19 ) + abs( R1 - R13 ) + abs( R13 - R25 ) 
							+ abs( G7 - G13 ) + abs( G13 - G19 );

					B = ( deltaNE < deltaNW ) ? HueTransit( G9, G13, G17, B9, B17 ) 
							: HueTransit( G7, G13, G19, B7, B19 );
				}
				// синий пиксель
				if( x % 2 == 1 && y % 2 == 1 ) {
					int B13 = B;
					int B1 = GetPixel( x - 2, y - 2 ).B;
					int B5 = GetPixel( x + 2, y - 2 ).B;
					int B21 = GetPixel( x - 2, y + 2 ).B;
					int B25 = GetPixel( x + 2, y + 2 ).B;

					int R7 = GetPixel( x - 1, y - 1 ).R;
					int R9 = GetPixel( x + 1, y - 1 ).R;
					int R17 = GetPixel( x - 1, y + 1 ).R;
					int R19 = GetPixel( x + 1, y + 1 ).R;

					int G13 = G;
					int G7 = GetPixel( x - 1, y - 1 ).G;
					int G9 = GetPixel( x + 1, y - 1 ).G;
					int G17 = GetPixel( x - 1, y + 1 ).G;
					int G19 = GetPixel( x + 1, y + 1 ).G;

					int deltaNE = abs( R9 - R17 ) + abs( B5 - B13 ) + abs( B13 - B21 ) 
							+ abs( G9 - G13 ) + abs( G13 - G17 );
					int deltaNW = abs( R7 - R19 ) + abs( B1 - B13 ) + abs( B13 - B25 ) 
							+ abs( G7 - G13 ) + abs( G13 - G19 );

					R = ( deltaNE < deltaNW ) ? HueTransit( G9, G13, G17, R9, R17 ) : 
							HueTransit( G7, G13, G19, R7, R19 );
				}

				SetPixel( x, y, R, G, B );
			}
		}
	}
};

CImage& demosacing( BitmapData& pData )
{
	CImage image( pData );
	time_t start = clock();
	image.PPG();
	time_t end = clock();
	_tprintf( _T( "Time: %.3f\n" ), static_cast<double>( end - start ) / CLOCKS_PER_SEC );
	return image;
}

CImage& process( BitmapData& pData )
{
	return demosacing( pData );
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
	if( argc != 4 )
	{
		// <inputFile.bmp> = CFA.bmp 
		// <outputFile.bmp> = Proc_CFA.bmp 
		// <originalFile.bmp> = Original.bmp
		_tprintf( _T("Usage: BayerPattern <inputFile.bmp> <outputFile.bmp> <originalFile.bmp>\n") );
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

		CImage img = process( bmpData );

		Bitmap GDIBitmapOriginal( argv[3] );
		BitmapData bmpDataOriginal;
		if( Ok != GDIBitmapOriginal.LockBits( &rc, ImageLockModeRead | ImageLockModeWrite, PixelFormat24bppRGB, &bmpDataOriginal ) )
			_tprintf( _T( "Failed to lock image: %s\n" ), argv[1] );
		else
			_tprintf( _T( "File: %s\n" ), argv[1] );

		CImage original( bmpDataOriginal );

		_tprintf( _T( "PSNR = %f\n" ), img.Compare( original ) );

		GDIBitmap.UnlockBits( &bmpData );
		GDIBitmapOriginal.UnlockBits( &bmpDataOriginal );

		// Save result
		CLSID clsId;
		GetEncoderClsid( _T("image/bmp"), &clsId );
		GDIBitmap.Save( argv[2], &clsId, NULL );
	}

	GdiplusShutdown( gdiplusToken );
	system( "openProcImage.cmd" );
	system( "pause" );
	return 0;
}
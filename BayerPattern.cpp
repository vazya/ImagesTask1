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
		pBuffer( (BYTE *)pData.Scan0 )
	{
	}

	int GetAdr( int x, int y )
	{
		return bpr*y + bpp*x;
		//return bpr*bpp*y + bpp*x;
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
		if( x >= w ) {
			return w - 1;
		}
		return x;
	}
	int CheckY( int y )
	{
		if( y < 0 ) {
			return 0;
		}
		if( y >= h ) {
			return h - 1;
		}
		return y;
	}
	// возвращает пиксель из исходного изображения, делая безопасное обращение
	// а также учитывающие stride и прочее
	Pixel GetPixel( int x, int y )
	{
		x = CheckX( x );
		y = CheckY( y );
		//int adr = bpr*y + bpp*x;
		int adr = GetAdr( x, y );//bpr*bpp*y + bpp*x;
		return Pixel( pBuffer[adr + 2], pBuffer[adr + 1], pBuffer[adr] );
	}

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
		if( ( (l1 < l2) && (l2 < l3) ) || ( (l1 > l2) && (l2 > l3) ) ) {
			return v1 + (v3 - v3)*(l2 - l1) / (l3 - l1);
		} else {
			return (v1 + v3) / 2 + (l2 - (l1 + l3) / 2) / 2;
		}
		assert( false );
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
				Pixel pixel = GetPixel( x, y );
				// замечание, для исходной картинки эти три значения всегда равны
				int B = pixel.B;
				int G = pixel.G;
				int R = pixel.R;
				int Y = (LumaRed * R + LumaGreen * G + LumaBlue * B + (CoeffNormalization >> 1)) >> CoeffNormalizationBitsCount;
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

					int delta = Min( deltaN, deltaE, deltaW, deltaS );
					switch( delta ) {
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

					int delta = Min( deltaN, deltaE, deltaW, deltaS );
					switch( delta ) {
						case 0: {
							G = (G8 * 3 + G18 + B13 - B3) / 4;
							break;
						}
						case 1: {
							G = (G14 * 3 + G12 + B13 - B15) / 4;
							break;
						}
						case 2: {
							G = (G12 * 3 + G14 + B13 - B11) / 4;
							break;
						}
						case 3: {
							G = (G18 * 3 + G8 + B13 - B23) / 4;
							break;
						}
						default:
							assert( false );
					}
				}

				int pixelAdr = GetAdr( x, y );
				//pBuffer[pixelAdr] = B; (Y + B) >> 1;
				pBuffer[pixelAdr + 1] = CheckValue( G ); // (Y + G) >> 1;
				//pBuffer[pixelAdr + 2] = R; (Y + R) >> 1;
			}
		}
		// восстановление R и B в G пикселях
		//*
		for( int y = top; y < bot; y++ ) {
			for( int x = left; x < right; x++ ) {
				Pixel pixel = GetPixel( x, y );
				// замечание, для исходной картинки эти три значения всегда равны
				int B = pixel.B;
				int G = pixel.G;
				int R = pixel.R;
				int Y = (LumaRed * R + LumaGreen * G + LumaBlue * B + (CoeffNormalization >> 1)) >> CoeffNormalizationBitsCount;
				// зеленый пиксель
				if( (x % 2 == 1 && y % 2 == 0) || (x % 2 == 0 && y % 2 == 1) ) {
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

				int pixelAdr = GetAdr( x, y );
				pBuffer[pixelAdr] = CheckValue( B ); //(Y + B) >> 1;
				//pBuffer[pixelAdr + 1] = G; // (Y + G) >> 1;
				pBuffer[pixelAdr + 2] = CheckValue( R ); // (Y + R) >> 1;
			}
		}
		//*/
		// восстановление R и B в B и R пикселях
		//*
		for( int y = top; y < bot; y++ ) {
			for( int x = left; x < right; x++ ) {
				Pixel pixel = GetPixel( x, y );
				// замечание, для исходной картинки эти три значения всегда равны
				int B = pixel.B;
				int G = pixel.G;
				int R = pixel.R;
				int Y = (LumaRed * R + LumaGreen * G + LumaBlue * B + (CoeffNormalization >> 1)) >> CoeffNormalizationBitsCount;
				// красный пиксель - для этих пикселей был красный детектор интенсивности
				if( x % 2 == 0 && y % 2 == 0 ) {
					int R13 = R;
					int R1 = GetPixel( x - 2, y - 2 ).R;
					int R5 = GetPixel( x + 2, y + 2 ).R;
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

					int deltaNE = abs( B9 - B17 ) + abs( R5 - R13 ) + abs( R13 - R21 ) + abs( G9 - G13 ) + abs( G13 - G17 );
					int deltaNW = abs( B7 - B19 ) + abs( R1 - R13 ) + abs( R13 - R25 ) + abs( G7 - G13 ) + abs( G13 - G19 );

					if( deltaNE < deltaNW ) {
						B = HueTransit( G9, G13, G17, B9, B17 );
					} else {
					//if( deltaNW < deltaNE ) {
						B = HueTransit( G7, G13, G19, B7, B19 );
					}
				}
				// синий пиксель
				if( x % 2 == 1 && y % 2 == 1 ) {
					int B13 = B;
					int B1 = GetPixel( x - 2, y - 2 ).B;
					int B5 = GetPixel( x + 2, y + 2 ).B;
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

					int deltaNE = abs( R9 - R17 ) + abs( B5 - B13 ) + abs( B13 - B21 ) + abs( G9 - G13 ) + abs( G13 - G17 );
					int deltaNW = abs( R7 - R19 ) + abs( B1 - B13 ) + abs( B13 - B25 ) + abs( G7 - G13 ) + abs( G13 - G19 );

					if( deltaNE < deltaNW ) {
						R = HueTransit( G9, G13, G17, R9, R17 );
					} else {
						//if( deltaNW < deltaNE ) {
						R = HueTransit( G7, G13, G19, R7, R19 );
					}
				}
				int pixelAdr = GetAdr( x, y );
				pBuffer[pixelAdr] = CheckValue( B ); // (Y + B) >> 1;
				//pBuffer[pixelAdr + 1] = G; // (Y + G) >> 1;
				pBuffer[pixelAdr + 2] = CheckValue( R ); // (Y + R) >> 1;
			}
		}
		//*/
	}
	// Average Nearest Neighbor - усреднение по ближайшим соседям-пикселям искомого цвета
	void ANN( int top = 0, int bot = 0, int left = 0, int right = 0 )
	{
		Bound bound( CheckBound( Bound( top, bot, left, right ) ) );
		top = bound.top;
		bot = bound.bot;
		left = bound.left;
		right = bound.right;

		// int baseAdr = 0; // bpr*top + bpp*left; // 0;
		for( int y = top; y < bot; y++ ) {
			// int pixelAdr = baseAdr;
			for( int x = left; x < right; x++ ) {
				Pixel pixel = GetPixel( x, y );
				// замечание, для исходной картинки эти три значения всегда равны
				int B = pixel.B;
				int G = pixel.G;
				int R = pixel.R;
				int Y = (LumaRed * R + LumaGreen * G + LumaBlue * B + (CoeffNormalization >> 1)) >> CoeffNormalizationBitsCount; // luminance
				
				// красный пиксель - для этих пикселей был красный детектор интенсивности
				if( x % 2 == 0 && y % 2 == 0 ) {
					R = pixel.R; // правильное зачение

					// усредним синий по соседям
					int BTL = GetPixel( x - 1, y - 1 ).B; // Blue Top Left
					int BTR = GetPixel( x + 1, y - 1 ).B; // Blue Top Right
					int BBL = GetPixel( x - 1, y + 1 ).B; // Blue Bot Left
					int BBR = GetPixel( x + 1, y + 1 ).B; // Blue Bot Right
					B = int( (BTL + BTR + BBL + BBR) / 4 );

					// усредним зеленый по соседям
					int GT = GetPixel( x, y - 1 ).G; // Green Top
					int GB = GetPixel( x, y + 1 ).G; // Green Bot
					int GL = GetPixel( x - 1, y ).G; // Green Left
					int GR = GetPixel( x + 1, y ).G; // Green Right
					G = int( (GT + GB + GL + GR) / 4 );
				}
				// синий пиксель
				if( x % 2 == 1 && y % 2 == 1 ) {
					// усредним синий по соседям
					int RTL = GetPixel( x - 1, y - 1 ).R; // Red Top Left
					int RTR = GetPixel( x + 1, y - 1 ).R; // Red Top Right
					int RBL = GetPixel( x - 1, y + 1 ).R; // Red Bot Left
					int RBR = GetPixel( x + 1, y + 1 ).R; // Red Bot Right
					R = int( (RTL + RTR + RBL + RBR) / 4 );

					// усредним зеленый по соседям
					int GT = GetPixel( x, y - 1 ).G; // Green Top
					int GB = GetPixel( x, y + 1 ).G; // Green Bot
					int GL = GetPixel( x - 1, y ).G; // Green Left
					int GR = GetPixel( x + 1, y ).G; // Green Right
					G = int( (GT + GB + GL + GR) / 4 );
				}
				// зеленый пиксель
				if( (x % 2 == 1 && y % 2 == 0) || (x % 2 == 0 && y % 2 == 1) ) {
					// усредним синий по соседям
					int RT = GetPixel( x, y - 1 ).R; // Red Top
					int RB = GetPixel( x, y + 1 ).R; // Red Bot
					R = int( (RT + RB) / 2 );

					// усредним зеленый по соседям
					int BL = GetPixel( x - 1, y ).G; // Blue Left
					int BR = GetPixel( x + 1, y ).G; // Blue Right
					B = int( (BL + BR) / 2 );
				}
				// desaturation by 50%
				// no need to check for the interval [0..255]
				int pixelAdr = GetAdr( x, y );

				pBuffer[pixelAdr] = B; // (Y + B) >> 1;
				pBuffer[pixelAdr + 1] = G; // (Y + G) >> 1;
				pBuffer[pixelAdr + 2] = R; // (Y + R) >> 1;
			}
		}
	}
	void GrayscaleDemosacing( int top = 0, int bot = 0, int left = 0, int right = 0 )
	{
		Bound bound( CheckBound( Bound( top, bot, left, right ) ) );
		top = bound.top;
		bot = bound.bot;
		left = bound.left;
		right = bound.right;

		int baseAdr = bpr*top + bpp*left; // 0;
		for( int y = top; y < bot; y++ ) {
			int pixelAdr = baseAdr;
			for( int x = left; x < right; x++ ) {
				Pixel pixel = GetPixel( x, y );
				// замечание, для исходной картинки эти три значения всегда равны
				int B = pixel.B;
				int G = pixel.G;
				int R = pixel.R;
				int Y = (LumaRed * R + LumaGreen * G + LumaBlue * B + (CoeffNormalization >> 1))
					>> CoeffNormalizationBitsCount; // luminance

				// красный пиксель - для этих пикселей был красный детектор интенсивности
				if( x % 2 == 0 && y % 2 == 0 ) {
					R = pixel.R; // правильное зачение

					// усредним синий по соседям
					int BTL = GetPixel( x - 1, y - 1 ).B; // Blue Top Left
					int BTR = GetPixel( x + 1, y - 1 ).B; // Blue Top Right
					int BBL = GetPixel( x - 1, y + 1 ).B; // Blue Bot Left
					int BBR = GetPixel( x + 1, y + 1 ).B; // Blue Bot Right
					B = int( (BTL + BTR + BBL + BBR) / 4 );

					// усредним зеленый по соседям
					int GT = GetPixel( x, y - 1 ).G; // Green Top
					int GB = GetPixel( x, y + 1 ).G; // Green Bot
					int GL = GetPixel( x - 1, y ).G; // Green Left
					int GR = GetPixel( x + 1, y ).G; // Green Right
					G = int( (GT + GB + GL + GR) / 4 );
				}
				// синий пиксель
				if( x % 2 == 1 && y % 2 == 1 ) {
					// усредним синий по соседям
					int RTL = GetPixel( x - 1, y - 1 ).R; // Red Top Left
					int RTR = GetPixel( x + 1, y - 1 ).R; // Red Top Right
					int RBL = GetPixel( x - 1, y + 1 ).R; // Red Bot Left
					int RBR = GetPixel( x + 1, y + 1 ).R; // Red Bot Right
					R = int( (RTL + RTR + RBL + RBR) / 4 );

					// усредним зеленый по соседям
					int GT = GetPixel( x, y - 1 ).G; // Green Top
					int GB = GetPixel( x, y + 1 ).G; // Green Bot
					int GL = GetPixel( x - 1, y ).G; // Green Left
					int GR = GetPixel( x + 1, y ).G; // Green Right
					G = int( (GT + GB + GL + GR) / 4 );
				}
				// зеленый пиксель
				if( ( x % 2 == 1 && y % 2 == 0 ) || ( x % 2 == 0 && y % 2 == 1 ) ){
					// усредним синий по соседям
					int RT = GetPixel( x, y - 1 ).R; // Red Top
					int RB = GetPixel( x, y + 1 ).R; // Red Bot
					R = int( ( RT + RB ) / 2 );

					// усредним зеленый по соседям
					int BL = GetPixel( x - 1, y ).G; // Blue Left
					int BR = GetPixel( x + 1, y ).G; // Blue Right
					B = int( ( BL + BR ) / 2 );
				}
				// desaturation by 50%
				// no need to check for the interval [0..255]
				//pBuffer[pixelAdr] = ( Y + B ) >> 1;
				//pBuffer[pixelAdr + 1] = ( Y + G ) >> 1;
				//pBuffer[pixelAdr + 2] = ( Y + R ) >> 1;


				pBuffer[pixelAdr] = B; // (Y + B) >> 1;
				pBuffer[pixelAdr + 1] = G; // (Y + G) >> 1;
				pBuffer[pixelAdr + 2] = R; // (Y + R) >> 1;
				pixelAdr += bpp;
			}
			baseAdr += bpr;
		}
	}
};

void demosacing( BitmapData& pData )
{
	CImage image( pData );
	time_t start = clock();
	// 1322, 1543, 1717, 2467
	// image.ANN();
	//image.PPG( 1322, 1543, 1717, 2467 );
	image.PPG();
	image.PPG();
	//image.PPG();
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
		for( int x = 731; x < w; x++ ) {
			int p = 0;
			if( ( x == 731 ) && ( y == 2073 ) ) {
				int i = pixelAdr;
				system( "pause" );
			}

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
	//grayscaleDemosacing( pData );
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
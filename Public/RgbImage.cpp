#include <atlstr.h>
#include <ctime>
#include <math.h>

#include <libraw.h>

#using "mscorlib.dll"

using namespace System;
using namespace System::Drawing;
using namespace System::Runtime::InteropServices;

#include <Renderer.h>

#include <RgbImage.h>

namespace AstroPhoto {
namespace LibRaw {

RgbImage::RgbImage( int _width, int _height ) : width( _width ), height( _height )
{
	int count = 3 * width * height;
	rgbPixels = new unsigned int[count];
	memset( rgbPixels, 0, count * sizeof( unsigned int ) );
}

RgbImage::RgbImage( unsigned int* _rgbPixels, int _width, int _height ) :
	rgbPixels( _rgbPixels ), width( _width ), height( _height )
{
}

RgbImage::~RgbImage()
{
	this->!RgbImage();
	GC::SuppressFinalize( this );
}

RgbImage::!RgbImage()
{
	delete[] rgbPixels;
	rgbPixels = 0;
}

Bitmap^ RgbImage::RenderBitmap( Curve^ curve, int saturation )
{
	Bitmap^ bitmap = gcnew Bitmap( width, height, Imaging::PixelFormat::Format24bppRgb );
	Imaging::BitmapData^ bitmapData = bitmap->LockBits( System::Drawing::Rectangle( 0, 0, width, height ),
		Imaging::ImageLockMode::WriteOnly, bitmap->PixelFormat );
	try {
		RECT r = { 0, 0, width, height };
		
		Renderer renderer( rgbPixels, curve->R(), curve->G(), curve->B() );
		renderer.RenderBitmap( (unsigned char*)( bitmapData->Scan0.ToPointer() ), bitmapData->Stride, r, saturation );
	} finally {
		bitmap->UnlockBits( bitmapData );
	}
	
	return bitmap;
}

void RgbImage::Add( RgbImage^ rgbImage )
{
	int count = 3 * width * height;
	for( int i = 0; i < count; i++ ) {
		rgbPixels[i] += rgbImage->rgbPixels[i];
	}
}

cli::array<unsigned short>^ RgbImage::GetRgbPixels16()
{
	cli::array<unsigned short>^ pixels = gcnew cli::array<unsigned short>( height * 3 * width );
	std::vector<unsigned short> buf;
	buf.resize( 3 * width );
	unsigned short* line = &buf.front();
	for( int y = 0; y < height; y++ ) {
		unsigned int* src = rgbPixels + 3 * width * y;
		for( int i = 0; i < 3 * width; i++ ) {
			line[i] = src[i];
		}
		int destinationIndex = y * 3 * width;
		System::Runtime::InteropServices::Marshal::Copy( IntPtr( line ), 
			(cli::array<short>^)( pixels ), destinationIndex, 3 * width );
	}
	return pixels;
}

void RgbImage::BackgroundLevels( LImage^ _mask, [Out] int% BgR, [Out] int% BgG, [Out] int% BgB )
{
	unsigned int* mask = _mask->GetPixels();
	int pixelCount = width * height;
	int count = 0;
	double sumR = 0;
	double sumG = 0;
	double sumB = 0;
	for( int i = 0; i < pixelCount; i++ ) {
		if( mask[i] ) {
			int j = 3 * i;
			sumR += rgbPixels[j + 0];
			sumG += rgbPixels[j + 1];
			sumB += rgbPixels[j + 2];
			count++;
		}
	}
	BgR = (int)( sumR / count + 0.5 );
	BgG = (int)( sumG / count + 0.5 );
	BgB = (int)( sumB / count + 0.5 );
}

LImage^ RgbImage::CreateLImage()
{
	int count = width * height;
	unsigned int* lPixels = new unsigned int[count];
	for( int i = 0; i < count; i++ ) {
		int j = 3 * i;
		unsigned int R = rgbPixels[j + 0];
		unsigned int G = rgbPixels[j + 1];
		unsigned int B = rgbPixels[j + 2];
		lPixels[i] = R + G + B;
	}
	return gcnew LImage( lPixels, width, height );
}

Bitmap^ LImage::RenderBitmap( unsigned int max )
{
	Bitmap^ bitmap = gcnew Bitmap( width, height, Imaging::PixelFormat::Format24bppRgb );
	Imaging::BitmapData^ bitmapData = bitmap->LockBits( System::Drawing::Rectangle( 0, 0, width, height ),
		Imaging::ImageLockMode::WriteOnly, bitmap->PixelFormat );
	try {
		unsigned char* ptr = (unsigned char*)( bitmapData->Scan0.ToPointer() );
		for( int y = 0; y < height; y++ ) {
			int i0 = y * width;
			int j0 = y * bitmapData->Stride; 
			for( int x = 0; x < width; x++ ) {
				int i = i0 + x;
				int j = j0 + 3 * x;
				unsigned int L = lPixels[i];
				L = L > max ? 255 : ( L * 255 ) / max;
				ptr[j + 0] = L;
				ptr[j + 1] = L;
				ptr[j + 2] = L;
			}
		}
	} finally {
		bitmap->UnlockBits( bitmapData );
	}
	
	return bitmap;
}

LImage::LImage( cli::array<LImage^>^ layers, cli::array<int>^ thresholds )
{
	int levels = thresholds->Length;
	width = layers[0]->width;
	height = layers[0]->height;
	lPixels = new unsigned int[width * height];
	memcpy( lPixels, layers[layers->Length - 1]->lPixels, width * height * sizeof( unsigned int ) );
	for( int k = layers->Length - 2; k >= 0; k-- ) {
		unsigned int* pixels = layers[k]->lPixels;
		for( int i = 0; i < width * height; i++ ) {
			int v = (int)( pixels[i] ) - 512;
			if( k < levels ) {
				int threshold = thresholds[k];
				if( threshold == 0 || ( v > threshold || v < -threshold ) ) {
					// fall throuigh
					/*if( threshold > 0 && ( v < 3 * threshold || v > 3 * threshold ) ) {
						if( k == 0 ) {
							v += 2 * v;
						}
						if( k == 1 ) {
							v += v;
						}
					}*/
				} else {
					lPixels[i] = (int)( lPixels[i] ) + ( v * ( v * 4 ) ) / ( threshold * 4 );
					continue;
				}
			}
			lPixels[i] = (int)( lPixels[i] ) + v;
		}
	}
}

cli::array<LImage^>^ LImage::DoWaveletTransform( int count )
{
	cli::array<LImage^>^ layers = gcnew cli::array<LImage^>( count );
	
	unsigned int* pixels = new unsigned int[width * height];
	memcpy( pixels, lPixels, width * height * sizeof( unsigned int ) );
	for( int k = 0; k < count - 1; k++ ) {
		int d = 1 << k;
		int d2 = 2 * d;
		int dWidth = d * width;
		unsigned int* newPixels = new unsigned int[width * height];
		for( int y = d2; y < height - d2 - 1; y++ ) {
			int yWidth = y * width;
			for( int x = d2; x < width - d2 - 1; x++ ) {
				int i2 = yWidth + x;
				int i1 = i2 - dWidth;
				int i0 = i1 - dWidth;
				int i3 = i2 + dWidth;
				int i4 = i3 + dWidth;
				
				// 1/256	1/64	3/128	...
				// 1/64		1/16	3/32	...
				// 3/128	3/32	9/64	...
				// ...
				newPixels[i2] = (
					01 * pixels[i0 - d2] + 04 * pixels[i0 - d] + 06 * pixels[i0] + 04 * pixels[i0 + d] + 01 * pixels[i0 + d2] +
					04 * pixels[i1 - d2] + 16 * pixels[i1 - d] + 24 * pixels[i1] + 16 * pixels[i1 + d] + 04 * pixels[i1 + d2] +
					06 * pixels[i2 - d2] + 24 * pixels[i2 - d] + 36 * pixels[i2] + 24 * pixels[i2 + d] + 06 * pixels[i2 + d2] +
					04 * pixels[i3 - d2] + 16 * pixels[i3 - d] + 24 * pixels[i3] + 16 * pixels[i3 + d] + 04 * pixels[i3 + d2] +
					01 * pixels[i4 - d2] + 04 * pixels[i4 - d] + 06 * pixels[i4] + 04 * pixels[i4 + d] + 01 * pixels[i4 + d2] 
					) / 256;
			}
		}
		for( int i = 0; i < width * height; i++ ) {
			pixels[i] = int( pixels[i] ) - newPixels[i] + 512;
		}
		layers[k] = gcnew LImage( pixels, width, height );
		pixels = newPixels;
	}
	
	layers[layers->Length - 1] = gcnew LImage( pixels, width, height );

	return layers;
}

void LImage::MaskBackground( unsigned int maskValue, int kSigma )
{
	unsigned int pixelsCount = width * height;
	
	unsigned int maxValue = 4096;
	unsigned int* counts = new unsigned int[maxValue];
	for( unsigned int i = 0; i < maxValue; i++ ) {
		counts[i] = 0;
	}

	for( unsigned int i = 0; i < pixelsCount; i++ ) {
		unsigned int value = lPixels[i];
		if( value < maxValue ) {
			counts[value]++;
		}
	}
	
	int median = -1;
	unsigned int medianCount = -1;
	unsigned int sum = 0;
	for( unsigned int i = 0; i < maxValue; i++ ) {
		sum += counts[i];
		if( sum >= pixelsCount / 2 ) {
			median = i;
			medianCount = counts[i];
			break;
		}
	}
	
	int halfWidth = -1;
	for( unsigned int i = median; i < maxValue; i++ ) {
		if( counts[i] <= medianCount / 2 ) {
			halfWidth = i - median;
			break;
		}
	}

	delete[] counts;

	unsigned int threshold = median + kSigma * halfWidth;
	for( unsigned int i = 0; i < pixelsCount; i++ ) {
		unsigned int value = lPixels[i];
		if( value < threshold ) {
			lPixels[i] = maskValue;
		} else {
			lPixels[i] = ~maskValue;
		}
	}

}

} //namespace LibRaw
} // namespace AstroPhoto
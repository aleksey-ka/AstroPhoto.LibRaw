#pragma once

#include <Curves.h>

namespace AstroPhoto {
namespace LibRaw {

ref class LImage;

public ref class RgbImage {
public:
	RgbImage( int width, int height );
	RgbImage( unsigned int* rgbPixels, unsigned width, unsigned height );
	
	~RgbImage();
	
	Bitmap^ RenderBitmap( Curve^ curve, int saturation );

	void Add( RgbImage^ rgbImage );

	LImage^ CreateLImage();

	void BackgroundLevels( LImage^ mask, [Out] int% BgR, [Out] int% BgG, [Out] int% BgB );

private:
	unsigned int* rgbPixels;
	unsigned width;
	unsigned height;
};

public ref class LImage {
public:
	LImage( unsigned int* _lPixels, unsigned _width, unsigned _height ) :
		lPixels( _lPixels ), width( _width ), height( _height )
	{
	}

	LImage( cli::array<LImage^>^ layers, cli::array<int>^ thresholds ); 

	~LImage()
	{
		delete[] lPixels;
	}

	cli::array<LImage^>^ DoWaveletTransform( int count );

	void MaskBackground( unsigned int maskValue, int kSigma );

	Bitmap^ RenderBitmap( int max );

	unsigned int* GetPixels() { return lPixels; }

private:
	unsigned int* lPixels;
	unsigned width;
	unsigned height;
};

} //namespace LibRaw
} // namespace AstroPhoto
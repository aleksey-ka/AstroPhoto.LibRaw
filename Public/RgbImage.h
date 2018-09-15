#pragma once

#include <Curves.h>

namespace AstroPhoto {
namespace LibRaw {

ref class LImage;

public ref class RgbImage {
public:
	RgbImage( int width, int height );
	RgbImage( unsigned int* rgbPixels, int width, int height );
	
	~RgbImage();
	!RgbImage();
	
	Bitmap^ RenderBitmap( Curve^ curve, int saturation );

	void Add( RgbImage^ rgbImage );

	LImage^ CreateLImage();

	void BackgroundLevels( LImage^ mask, [Out] int% BgR, [Out] int% BgG, [Out] int% BgB );

private:
	int width;
	int height;
	unsigned int* rgbPixels;
};

public ref class LImage {
public:
	LImage( unsigned int* _lPixels, int _width, int _height ) :
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

	Bitmap^ RenderBitmap( unsigned int max );

	unsigned int* GetPixels() { return lPixels; }

private:
	int width;
	int height;
	unsigned int* lPixels;
};

} //namespace LibRaw
} // namespace AstroPhoto
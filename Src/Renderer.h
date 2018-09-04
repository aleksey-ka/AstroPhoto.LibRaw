#pragma once

class Renderer {
public:
	Renderer( unsigned short* _raw_image, int _raw_width, int _raw_height, unsigned int _idata_filters,
		unsigned char* _curveR, unsigned char* _curveG, unsigned char* _curveB );

	Renderer( unsigned int* _rgbPixels, unsigned char* _curveR, unsigned char* _curveG, unsigned char* _curveB );
	void RenderBitmapHalfRes( unsigned char* pixels, int stride, int saturation );
	void RenderBitmap( unsigned char* pixels, int stride, const RECT& rect, int saturation );

	unsigned int* CalculateRgbPixelValues( const RECT& rect );
	unsigned short* ExtractFiltered( const RECT& rect );

	void RenderCFA( unsigned char* pixels, int stride, const RECT& rect );

private:
	void renderBitmapHalfRes( unsigned char* pixels, int stride );
	void renderBitmapHalfResSaturation( unsigned char* pixels, int stride, int saturation );
	void renderBitmap( unsigned char* pixels, int stride, unsigned rectWidth, unsigned rectHeight );
	void renderBitmapSaturation( unsigned char* pixels, int stride, unsigned rectWidth, unsigned rectHeight, int saturation );

	void copySubtractOffset( unsigned short* raw_image, const RECT& rect );
	void filterSpikes( unsigned short* raw_image, const RECT& rect );
	unsigned* calculateRgbPixelValues( unsigned short* raw_image, const RECT& rect );

private:
	unsigned int idata_filters;
	unsigned short raw_width;
	unsigned short raw_height;
	unsigned int raw_count;
	unsigned short* raw_image;

	unsigned int* rgbPixels;

	unsigned char* curveR;
	unsigned char* curveG;
	unsigned char* curveB;
};
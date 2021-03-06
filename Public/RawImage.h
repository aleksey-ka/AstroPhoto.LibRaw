#pragma once

#include <Curves.h>

namespace AstroPhoto {
namespace LibRaw {

ref class RgbImage;

public ref class RawImage {
public:
	RawImage( libraw_data_t* data );
	RawImage( int width, int height, int channel, cli::array<unsigned short>^ pixels );
	RawImage( int width, int height, int channel, unsigned short* pixels );
	RawImage( RawImage^, RECT, int channel );
	RawImage( String^ filePath );

	~RawImage();
	!RawImage();

	property Image^ Preview
	{
		Image^ get() 
		{
			if( thumbnail_bytes != nullptr ) {
				System::IO::MemoryStream stream( thumbnail_bytes );
				return Image::FromStream( %stream );
			} else {
				return RenderBitmapHalfRes( nullptr, 0 );
			}
		}
	}

	property float IsoSpeed
	{
		float get()
		{ 
			return iso_speed;
		}
	}

	property float Shutter
	{
		float get()
		{ 
			return shutter;
		}
	}

	property int Height
	{
		int get()
		{ 
			return raw_height;
		}
	}

	property int Width
	{
		int get()
		{ 
			return raw_width;
		}
	}

	property String^ ImageInfo
	{
		String^ get()
		{ 
			return imageInfo;
		}
	}

	Bitmap^ RenderBitmapHalfRes( Curve^ curve, int saturation );
	Bitmap^ RenderBitmap( System::Drawing::Rectangle rect, Curve^ curve, int saturation );
	
	RgbImage^ ExtractRgbImage( System::Drawing::Rectangle rect );
	RawImage^ ExtractRawImage( System::Drawing::Rectangle rect );
	Bitmap^ RenderCFA( System::Drawing::Rectangle rect, Curve^ curve );

	void SaveCFA( String^ filePath );

	Bitmap^ GetHistogram();

	void CalcStatistics( int ch, int _x, int _y, int _width, int _height, [Out] double% mean, [Out] double% sigma );

	cli::array<unsigned short>^ GetRawPixels()
	{
		cli::array<unsigned short>^ pixels = gcnew cli::array<unsigned short>( raw_count );
		System::Runtime::InteropServices::Marshal::Copy( IntPtr( raw_image ), 
			(cli::array<short>^)( pixels ), 0, raw_count );
		return pixels;
	}

	cli::array<unsigned short>^ GetRawPixels( System::Drawing::Rectangle rect )
	{
		int x = rect.X;
		int y = rect.Y;
		int width = rect.Width;
		int height = rect.Height;

		cli::array<unsigned short>^ pixels = gcnew cli::array<unsigned short>( width * height );
		for( int i = 0; i < height; i++ ) {
			unsigned short* src = raw_image + ( y + i ) * raw_width + x;
			int destinationIndex = i * width;
			System::Runtime::InteropServices::Marshal::Copy( IntPtr( src ), (cli::array<short>^)( pixels ), destinationIndex, width );
		}
		return pixels;
	}

	cli::array<__int64>^ GetRawPixelsApplyFlat( System::Drawing::Rectangle rect, cli::array<unsigned short>^ flat, int multiplier );
	cli::array<__int64>^ GetRawPixelsApplyFlatDark( System::Drawing::Rectangle rect, 
		cli::array<unsigned short>^ flat, cli::array<unsigned short>^ dark, int multiplier );

	cli::array<unsigned char>^ GetBytes()
	{
		cli::array<unsigned char>^ bytes = gcnew cli::array<unsigned char>( raw_count * 2 );
		System::Runtime::InteropServices::Marshal::Copy( IntPtr( raw_image ), bytes, 0, raw_count * 2 );
		return bytes;
	}

	int Channel( int x, int y ) 
	{
		int cindex = ( idata_filters >> (((y << 1 & 14) | (x & 1)) << 1) & 3);
		return cindex;
	}
	
	property unsigned int data_filters
	{
		unsigned int get()
		{ 
			return idata_filters;
		}
	}

	wchar_t Color( int channel )
	{
		static wchar_t* rgbg = L"RGBG";
		return rgbg[channel];
	}

	void ApplyFlat( cli::array<unsigned short>^ pixels, int multiplier );
	void ApplyDark( cli::array<unsigned short>^ pixels );

	const unsigned short* _raw_image() { return raw_image; }

private:
	float iso_speed;
	float shutter;
	time_t timestamp;
	unsigned int color_maximum;
	unsigned int idata_filters;

	String^ imageInfo;

	cli::array<unsigned char>^ thumbnail_bytes;

	int raw_width;
	int raw_height;
	int raw_count;
	unsigned short* raw_image;
	
	int source_rect_left;
	int source_rect_top;
	int source_rect_right;
	int source_rect_bottom;

	void init( int width, int height, int channel );
	void createImageInfo( libraw_data_t* data );
	void createImageInfo();
};

public ref class RawBuffer32 {
public:
	RawBuffer32() : buffer( 0 ), width( 0 ), height( 0 ), count( 0 ) {}

	~RawBuffer32();

	void Add( RawImage^ rawImage );
	void Add2( RawImage^ rawImage );

	RawImage^ GetResult();

private:
	int count;
	int width;
	int height;
	unsigned int* buffer;
	unsigned short* result;
	float* sumSqr;
	int margin;
};

} //namespace LibRaw
} // namespace AstroPhoto

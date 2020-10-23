#include <atlstr.h>
#include <ctime>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <assert.h>

#include <string>
#include <iostream>
#include <sstream>
#include <map>

#include <libraw.h>

#using "mscorlib.dll"

using namespace System;
using namespace System::Drawing;
using namespace System::Runtime::InteropServices;

#include "RgbImage.h"
#include "Renderer.h"

#include "RawImage.h"

namespace AstroPhoto {
namespace LibRaw {

static unsigned int idata_filters_from_channel( int channel )
{
	switch( channel ) {
		case 0:	return 0xb4b4b4b4;
		case 1:	return 0xe1e1e1e1;
		case 2:	return 0x1e1e1e1e;
		case 3:	return 0x4b4b4b4b;
	}
	assert( false );
	return 0;
}

static unsigned int idata_filters_from_pattern( const char* pattern )
{
	switch( pattern[0] ) {
		case 'R' : return 0xb4b4b4b4; // RGGB
		case 'B' : return 0x1e1e1e1e; // BGGR
		case 'G' : return pattern[1] == 'R' ? 0xe1e1e1e1 : 0x4b4b4b4b; // GRBG GBRG
	}
	assert( false );
	return 0;
}

static const char* pattern_from_idata_filters( unsigned int idata_filters )
{
	switch( idata_filters ) {
		case 0xb4b4b4b4: return "RGGB";
		case 0xe1e1e1e1: return "GRBG";
		case 0x1e1e1e1e: return "BGGR";
		case 0x4b4b4b4b: return "GBRG";
	}
	assert( false );
	return 0;
}

RawImage::RawImage( libraw_data_t* data )
{
	raw_width = data->sizes.raw_width;
	raw_height = data->sizes.raw_height;
	iso_speed = data->other.iso_speed;
	shutter = data->other.shutter;
	timestamp = data->other.timestamp;
	color_maximum = data->color.maximum;
	idata_filters = data->idata.filters;
	
	source_rect_left = 0;
	source_rect_top = 0;
	source_rect_right = 0;
	source_rect_bottom = 0;

	createImageInfo( data );
	
	if( data->thumbnail.thumb != 0 ) {
		thumbnail_bytes = gcnew cli::array<unsigned char>( data->thumbnail.tlength );
		System::Runtime::InteropServices::Marshal::Copy( IntPtr( data->thumbnail.thumb ), thumbnail_bytes, 0, data->thumbnail.tlength );
	} else {
		thumbnail_bytes = nullptr;
	}
	
	if( data->rawdata.raw_image != 0 ) {
		raw_count = raw_width * raw_height;
		raw_image = new unsigned short[raw_count];
		memcpy( raw_image, data->rawdata.raw_image, raw_count * sizeof( unsigned short ) );
	} else {
		raw_image = 0;
	}
}

RawImage::RawImage( RawImage^ src, RECT rect, int channel )
{
	raw_width = rect.right - rect.left;
	raw_height = rect.bottom - rect.top;
	iso_speed = src->iso_speed;
	shutter = src->shutter;
	timestamp = src->timestamp;
	color_maximum = src->color_maximum;
	idata_filters = idata_filters_from_channel( channel );

	source_rect_left = rect.left;
	source_rect_top = rect.top;
	source_rect_right = rect.right;
	source_rect_bottom = rect.bottom;

	imageInfo = src->imageInfo;

	thumbnail_bytes = nullptr;

	raw_count = raw_width * raw_height;
	raw_image = new unsigned short[raw_count];

	int width = raw_width;
	int heigth = raw_height;
	for( int i = 0; i < heigth; i++ ) {
		unsigned short* _src = src->raw_image + ( ( rect.top + i ) * src->raw_width + rect.left );
		unsigned short* dest = raw_image + i * width;
		for( int j = 0; j < width; j++ ) {
			dest[j] = _src[j];
		}
	}
}

RawImage::RawImage( String^ _filePath )
{
	CStringW filePath( _filePath );
	CStringW ext = filePath.Mid( filePath.ReverseFind( L'.' ) ).MakeLower();
	if( ext == L".cfa" ) {
		CStringW infoFilePath( filePath );
		infoFilePath.Replace( L".cfa", L".info" );

		std::map<std::wstring, std::wstring> map;
		
		std::wifstream info( infoFilePath );
		std::wstring line; 
		while( std::getline( info, line ) ) {
			size_t pos = line.find_first_of( L" \t" );
			assert( pos != std::wstring::npos );
			map.insert( std::pair<std::wstring, std::wstring>( 
				line.substr( 0, pos ),
				line.substr( pos + 1 ) ) );
		}
		info.close();

		int bits_per_pixel;
		swscanf( map[L"BITS_PER_PIXEL"].c_str(), L"%d", &bits_per_pixel );
		assert( bits_per_pixel == 16 );
		swscanf( map[L"WIDTH"].c_str(), L"%d", &raw_width );
		swscanf( map[L"HEIGHT"].c_str(), L"%d", &raw_height );
		swscanf( map[L"ISO"].c_str(), L"%f", &iso_speed );
		swscanf( map[L"EXPOSURE"].c_str(), L"%f", &shutter );
		swscanf( map[L"TIMESTAMP"].c_str(), L"%I64d", &timestamp );
		swscanf( map[L"SOURCE_RECT"].c_str(), L"%d,%d,%d,%d", &source_rect_left, 
			&source_rect_top, &source_rect_right, &source_rect_bottom );

		char pattern[5];
		swscanf( map[L"PATTERN"].c_str(), L"%S", &pattern );
		idata_filters = idata_filters_from_pattern( pattern );

		createImageInfo();
		
		raw_count = raw_width * raw_height;
		raw_image = new unsigned short[raw_count];

		FILE* in = _wfopen( filePath, L"rb" );	
		fread( raw_image, sizeof( unsigned short ), raw_count, in );
		fclose( in );
	} else {
		assert( ext == ".fit" );
		FILE* in = _wfopen( filePath, L"rb" );

		std::map<std::string, std::string> map;
		for( int i = 0; i < 36; i++ ) {
			char buf[81];
			buf[80] = 0;
			fread( buf, sizeof( char ), 80, in );
			
			std::string line( buf );

			if( line.rfind( "COMMENT", 0 ) == 0 || 
				line.rfind( "HISTORY", 0 ) == 0 ||
				line.rfind( "END", 0 ) == 0 ) {
				continue;
			}
			if( line.find_first_not_of( " " ) == std::string::npos ) {
				continue;
			}

			size_t pos = line.find( "=" );
			assert( pos != std::string::npos );

			size_t pos1 = line.find_last_not_of( " ", pos - 1 );
			size_t pos2 = line.find_first_not_of( " ", pos + 1 );
			size_t pos3 = line.find_first_of( "/", pos + 1 );
			if( pos3 != std::string::npos ) {
				pos3 = line.find_last_not_of( " ", pos3 - 1 );
			} else {
				pos3 = line.find_last_not_of( " " );
			}
			
			map.insert( std::pair<std::string, std::string>( 
				line.substr( 0, pos1 + 1 ),
				line.substr( pos2, pos3 - pos2 + 1 ) ) );
		}

		assert( map["SIMPLE"] == "T" );

		int bits_per_pixel;
		sscanf( map["BITPIX"].c_str(), "%d", &bits_per_pixel );
		assert( bits_per_pixel == 16 );

		int naxis;
		sscanf( map["NAXIS"].c_str(), "%d", &naxis );
		assert( naxis == 2 );

		sscanf( map["NAXIS1"].c_str(), "%d", &raw_width );
		sscanf( map["NAXIS2"].c_str(), "%d", &raw_height );

		int bzero;
		sscanf( map["BZERO"].c_str(), "%d", &bzero );
		int bscale;
		sscanf( map["BSCALE"].c_str(), "%d", &bscale );
		assert( bscale == 1 );

		sscanf( map["EXPOSURE"].c_str(), "%f", &shutter );
		sscanf( map["GAIN"].c_str(), "%f", &iso_speed );

		idata_filters = idata_filters_from_pattern( "RGGB" );
		timestamp = 0;
		source_rect_left = 0; 
		source_rect_top = 0;
		source_rect_right = 0;
		source_rect_bottom = 0;

		raw_count = raw_width * raw_height;
		raw_image = new unsigned short[raw_count];

		fread( raw_image, sizeof( unsigned short ), raw_count, in );
		fclose( in );

		unsigned char* ptr = (unsigned char*)raw_image;
		for( int i = 0; i < raw_count * 2; i += 2 ) {
			std::swap( ptr[i], ptr[i + 1] );
		}
		for( int y = 0; y < raw_height; y++ ) {
			for( int x = 0; x < raw_width; x++ ) {
				int i = y * raw_width + x;
				raw_image[i] += bzero;
				raw_image[i] -= 200;
				if( y % 2 == 0 ) {
					if( x % 2 == 0 ) {
						raw_image[i] /= 75; // R
					} else {
						raw_image[i] /= 48; // G
					}
				} else {
					if( x % 2 == 0 ) {
						raw_image[i] /= 48; // G
					} else {
						raw_image[i] /= 55; // B
					}
				}
				raw_image[i] += 512;
			}
		}
	}
}

void fwprintf_no_trailing_zeroes( FILE* file, const wchar_t* name, float value )
{
	char buf[50];
	sprintf_s( buf, sizeof( buf ), "%f", value );
	int pos = strlen( buf );
	for( int i = pos - 1; i > 0; i-- ) {
		if( buf[i] == '.' ) {
			buf[i] = 0;
			break;
		}
		if( buf[i] != '0' ) {
			buf[i + 1] = 0;
			break;
		}
	}
	fwprintf( file, L"%s %S\n", name, buf );
}

void RawImage::SaveCFA( String^ filePath )
{
	CStringW infoFilePath( filePath );
	infoFilePath.Replace( L".cfa", L".info" );
	FILE* info = _wfopen( infoFilePath, L"wt" );
	fwprintf( info, L"WIDTH %d\n", raw_width );
	fwprintf( info, L"HEIGHT %d\n", raw_height );
	fwprintf( info, L"BITS_PER_PIXEL 16\n" );
	fwprintf( info, L"OFFSET 512\n" );
	fwprintf( info, L"PATTERN %S\n", pattern_from_idata_filters( idata_filters ) );
	fwprintf_no_trailing_zeroes( info, L"ISO", iso_speed );
	fwprintf_no_trailing_zeroes( info, L"EXPOSURE", shutter );
	fwprintf( info, L"TIMESTAMP %I64d\n", timestamp );
	fwprintf( info, L"SOURCE_RECT %d,%d,%d,%d\n", source_rect_left,
		source_rect_top, source_rect_right, source_rect_bottom );
	fwprintf( info, L"SOURCE_FORMAT ARW\n" );
	fwprintf( info, L"CAMERA Sony NEX-5N\n" );
	fclose( info );

	FILE* out = _wfopen( CStringW( filePath ), L"wb" );
	fwrite( raw_image, sizeof( unsigned short ), raw_count, out );
	fclose( out );
}

RawImage::RawImage( int width, int height, int channel, cli::array<unsigned short>^ pixels ) 
{
	init( width, height, channel );
	raw_image = new unsigned short[raw_count];
	System::Runtime::InteropServices::Marshal::Copy( (cli::array<short>^)( pixels ), 0, IntPtr( raw_image ), raw_count );
}

RawImage::RawImage( int width, int height, int channel, unsigned short* pixels ) 
{
	init( width, height, channel );
	raw_image = pixels;
}

void RawImage::init( int width, int height, int channel )
{
	raw_width = width;
	raw_height = height;
	raw_count = raw_width * raw_height;
	idata_filters = idata_filters_from_channel( channel );
}

RawImage::~RawImage()
{
	this->!RawImage();
	GC::SuppressFinalize( this );
}

RawImage::!RawImage()
{
	delete[] raw_image;
	raw_image = 0;
	thumbnail_bytes = nullptr;
}

Bitmap^ RawImage::RenderBitmapHalfRes( Curve^ curve, int saturation )
{
	int width = raw_width / 2;
	int height = raw_height / 2;

	Bitmap^ bitmap = gcnew Bitmap( width, height, Imaging::PixelFormat::Format24bppRgb );
	Imaging::BitmapData^ bitmapData = bitmap->LockBits( System::Drawing::Rectangle( 0, 0, width, height ),
		Imaging::ImageLockMode::WriteOnly, bitmap->PixelFormat );
	try {
		unsigned char* curveR = curve != nullptr ? curve->R() : 0;
		unsigned char* curveG = curve != nullptr ? curve->G() : 0;
		unsigned char* curveB = curve != nullptr ? curve->B() : 0;
		Renderer renderer( raw_image, raw_width, raw_height, idata_filters, curveR, curveG, curveB );
		renderer.RenderBitmapHalfRes( (unsigned char*)( bitmapData->Scan0.ToPointer() ), bitmapData->Stride, saturation );
	} finally {
		bitmap->UnlockBits( bitmapData );
	}
	return bitmap;
}

Bitmap^ RawImage::RenderBitmap( System::Drawing::Rectangle rect, Curve^ curve, int saturation )
{
	Bitmap^ bitmap = gcnew Bitmap( rect.Width, rect.Height, Imaging::PixelFormat::Format24bppRgb );
	Imaging::BitmapData^ bitmapData = bitmap->LockBits( System::Drawing::Rectangle( 0, 0, rect.Width, rect.Height ),
		Imaging::ImageLockMode::WriteOnly, bitmap->PixelFormat );
	try {
		RECT r = { rect.Left, rect.Top, rect.Right, rect.Bottom };

		Renderer renderer( raw_image, raw_width, raw_height, idata_filters, curve->R(), curve->G(), curve->B() );
		renderer.RenderBitmap( (unsigned char*)( bitmapData->Scan0.ToPointer() ), bitmapData->Stride, r, saturation );
	} finally {
		bitmap->UnlockBits( bitmapData );
	}
	
	return bitmap;
}

RgbImage^ RawImage::ExtractRgbImage( System::Drawing::Rectangle rect )
{
	RECT r = { rect.Left, rect.Top, rect.Right, rect.Bottom };
	
	Renderer renderer( raw_image, raw_width, raw_height, idata_filters, 0, 0, 0 );
	return gcnew RgbImage( renderer.CalculateRgbPixelValues( r ), rect.Width, rect.Height );
}

RawImage^ RawImage::ExtractRawImage( System::Drawing::Rectangle rect )
{
	RECT r = { rect.Left, rect.Top, rect.Right, rect.Bottom };
	return gcnew RawImage( this, r, Channel( r.left, r.top ) );
}

Bitmap^ RawImage::RenderCFA( System::Drawing::Rectangle rect, Curve^ curve )
{
	Bitmap^ bitmap = gcnew Bitmap( rect.Width, rect.Height, Imaging::PixelFormat::Format24bppRgb );
	Imaging::BitmapData^ bitmapData = bitmap->LockBits( System::Drawing::Rectangle( 0, 0, rect.Width, rect.Height ),
		Imaging::ImageLockMode::WriteOnly, bitmap->PixelFormat );
	try {
		RECT r = { rect.Left, rect.Top, rect.Right, rect.Bottom };

		Renderer renderer( raw_image, raw_width, raw_height, idata_filters, curve->R(), curve->G(), curve->B() );
		renderer.RenderCFA( (unsigned char*)( bitmapData->Scan0.ToPointer() ), bitmapData->Stride, r );
	} finally {
		bitmap->UnlockBits( bitmapData );
	}
	return bitmap;
}

Bitmap^ RawImage::GetHistogram()
{
	int width = raw_width;
	int height = raw_height;

	unsigned int max = color_maximum;

	unsigned long* buf = new unsigned long[max + 1];
	memset( buf, 0, ( max + 1 ) * sizeof( unsigned long ) );

	for( int y = 0; y < height; y++ ) {
		int stride = y * width;
		for( int x = 0; x < width; x++ ) {
			unsigned int value = raw_image[stride + x];
			buf[value]++;
		}
	}

	unsigned long hwidth = max + 1;

	unsigned long maxBuf = 0;
	double* values = new double[hwidth];
	for( unsigned int x = 0; x < hwidth; x++ ) {
		unsigned long count = buf[x];
		values[x] = 231.4079 * ( log( count /10.0 + 10.0 ) - 1 );
		if( count > maxBuf ) {
			maxBuf = count;
		}
	}

	unsigned long hheight = (unsigned long)( 231.4079 * ( log( maxBuf/10.0 + 10.0 ) - 1 ) );

	Bitmap^ bitmap = gcnew Bitmap( hwidth, hheight, Imaging::PixelFormat::Format24bppRgb );
	Imaging::BitmapData^ bitmapData = bitmap->LockBits( System::Drawing::Rectangle( 0, 0, hwidth, hheight ),
		Imaging::ImageLockMode::WriteOnly, bitmap->PixelFormat );
	try {
		unsigned char* ptr = (unsigned char*)( bitmapData->Scan0.ToPointer() );
		for( unsigned int y = 0; y < hheight; y++ ) {
			int stride = y * bitmapData->Stride; 
			for( unsigned int x = 0; x < hwidth; x++ ) {
				unsigned char c = y - 80 > hheight - values[x] ? 0xA0 : 0x00;
				int i = stride + 3 * x;
				ptr[i + 0] = c;
				ptr[i + 1] = x % 20 == 0 ? 0 : c;
				ptr[i + 2] = x % 100 == 0 ? 0 : c;
			}
		}
	} finally {
		bitmap->UnlockBits( bitmapData );
	}
	delete[] buf;
	delete[] values;
	return bitmap;
}

void RawImage::CalcStatistics( int ch, int _x, int _y, int _width, int _height, [Out] double% mean, [Out] double% sigma )
{
	if( _y < 0 ) { 
		_height += _y; 
		_y = 0;
	}
	if( _y + _height > raw_height ) _height = raw_height - _y;
	if( _x < 0 ) {
		_width += _x;
		_x = 0;
	}
	if( _x + _width > raw_width ) _width = raw_width - _x;


	double _mean = 0;
	int N = 0;
	for( int y = 0; y < _height ; y++ ) {
		int Y = y + _y;
		for( int x = 0; x < _width; x++ ) {
			int X = x + _x;
			int cindex = ( idata_filters >> (((Y << 1 & 14) | (X & 1)) << 1) & 3);
			if( cindex == ch ) {
				_mean += raw_image[Y * raw_width + X];
				N++;
			}
		}
	}
	_mean /= N;

	double sumSqrDev = 0;
	for( int y = 0; y < _height ; y++ ) {
		int Y = y + _y;
		for( int x = 0; x < _width; x++ ) {
			int X = x + _x;
			int cindex = ( idata_filters >> (((Y << 1 & 14) | (X & 1)) << 1) & 3);
			if( cindex == ch ) {
				double dev = _mean - raw_image[Y * raw_width + X];
				sumSqrDev += dev * dev;
			}
		}
	}
	mean = _mean;
	sigma = sqrt( sumSqrDev / ( N - 1 ) );
}

void RawImage::createImageInfo()
{ 
	char buf[1000];
	std::string str;

	sprintf( buf, "ISO%.0f - %f s\n", iso_speed, shutter );
	str += buf;
	time_t t = timestamp;
	strftime( buf, sizeof( buf ), "TIMESTAMP: %d-%m-%Y %H:%M:%S\n", localtime( &t ) );		
	str += buf;

	imageInfo = gcnew String( str.c_str() );
}

void RawImage::createImageInfo( libraw_data_t* data )
{ 
	char buf[1000];
	std::string str;

	sprintf( buf, "= other =========================\n" );
	str += buf;
	sprintf( buf, "ISO%.0f - %f s - F%f - f = %f mm\n", data->other.iso_speed, data->other.shutter, 
		data->other.aperture, data->other.focal_len );
	str += buf;
	strftime( buf, sizeof( buf ), "TIMESTAMP: %d-%m-%Y %H:%M:%S\n", localtime( &data->other.timestamp ) );		
	str += buf;
	sprintf( buf, "ARTIST: \"%s\" DESCRIPTION: \"%s\"\n", data->other.artist, data->other.desc );
	str += buf;
	sprintf( buf, "SHOT_ORDER: %u\n", data->other.shot_order, data->other.gpsdata );
	str += buf;
	
	char* ptr = buf;
	ptr += sprintf( ptr, "GPS_DATA:\n" );
	for( int i = 0; i < 32; i++ ) {
		ptr += sprintf( ptr, "%.8x ", data->other.gpsdata[i] );
	}
	sprintf( ptr, "\n" );
	str += buf;

	sprintf( buf, "= idata =========================\n" );
	str += buf;
	sprintf( buf, "CAMERA: %s %s\n", data->idata.make, data->idata.model );
	str += buf;
	sprintf( buf, "RAW_COUNT: %u\n", data->idata.raw_count );
	str += buf;
	sprintf( buf, "COLORS: %d %c%c%c%c\n", data->idata.colors, data->idata.cdesc[0], data->idata.cdesc[1], data->idata.cdesc[2], data->idata.cdesc[3] );
	str += buf;

	ptr = buf;
	for( int col = 0; col < 6; col++ ) {
		for( int row = 0; row < 6; row++ ) {
			int cindex = (data->idata.filters >> (((row << 1 & 14) | (col & 1)) << 1) & 3);
			char cdesc = data->idata.cdesc[cindex];
			ptr += sprintf( ptr, "%c ", cdesc );
		}
		ptr += sprintf( ptr, "\n" );
	}		
	str += buf;

	sprintf( buf, "= sizes =========================\n" );
	str += buf;
	sprintf( buf, "SIZE: %hux%hu\n", data->sizes.height, data->sizes.width );
	str += buf;
	sprintf( buf, "RAW_SIZE: %hux%hu\n", data->sizes.raw_height, data->sizes.raw_width );
	str += buf;
	sprintf( buf, "iSIZE: %hux%hu\n", data->sizes.iheight, data->sizes.iwidth );
	str += buf;

	imageInfo = gcnew String( str.c_str() );
}

cli::array<__int64>^ RawImage::GetRawPixelsApplyFlat( System::Drawing::Rectangle rect, cli::array<unsigned short>^ flat, int multiplier )
{
	int x = rect.X;
	int y = rect.Y;
	int width = rect.Width;
	int height = rect.Height;

	RECT r = { x, y, x + width, y + height };

	Renderer renderer( raw_image, raw_width, raw_height, idata_filters, 0, 0, 0 );
	unsigned short* _raw_image = renderer.ExtractFiltered( r );

	cli::array<__int64>^ pixels = gcnew cli::array<__int64>( width * height );
	for( int i = 0; i < height; i++ ) {
		int i0 = ( y + i ) * raw_width + x;
		int iwidth = i * width;
		unsigned short* src = _raw_image + i0;
		for( int j = 0; j < width; j++ ) {
			__int64 value = (__int64)src[j]/* - offset*/;
			if( flat != nullptr ) {
				__int64 flatValue = ( ( ( value * multiplier ) * USHRT_MAX ) / flat[i0 + j] )/* + offset*/;
				pixels[iwidth + j] = flatValue;
			} else {
				__int64 flatValue = ( value * multiplier )/* + offset*/;
				pixels[iwidth + j] = flatValue;
			}
		}
	}

	delete[] _raw_image;

	return pixels;
}

cli::array<__int64>^ RawImage::GetRawPixelsApplyFlatDark( System::Drawing::Rectangle rect, 
	cli::array<unsigned short>^ flat, cli::array<unsigned short>^ dark, int multiplier )
{
	int x = rect.X;
	int y = rect.Y;
	int width = rect.Width;
	int height = rect.Height;

	RECT r = { x, y, x + width, y + height };

	//Renderer renderer( raw_image, raw_width, raw_height, idata_filters, 0, 0, 0 );
	//unsigned short* _raw_image = renderer.ExtractFiltered( r );

	cli::array<__int64>^ pixels = gcnew cli::array<__int64>( width * height );
	for( int i = 0; i < height; i++ ) {
		int i0 = ( y + i ) * raw_width + x;
		int iwidth = i * width;
		unsigned short* src = raw_image + i0;
		for( int j = 0; j < width; j++ ) {
			__int64 value = (__int64)src[j] - dark[i0 + j]/* - offset*/;
			if( flat != nullptr ) {
				__int64 flatValue = ( ( ( value * multiplier ) * USHRT_MAX ) / flat[i0 + j] )/* + offset*/;
				pixels[iwidth + j] = flatValue;
			} else {
				__int64 flatValue = ( value * multiplier )/* + offset*/;
				pixels[iwidth + j] = flatValue;
			}
		}
	}

	return pixels;
}

void RawImage::ApplyFlat( cli::array<unsigned short>^ pixels, int multiplier )
{
	for( int i = 0; i < raw_count; i++ ) {
		int value = (int)raw_image[i] - offset;
		int flatValue = ( ( ( value * multiplier ) * USHRT_MAX ) / pixels[i] ) + offset;
		if( flatValue < 0 ) {
			raw_image[i] = 0;
		} else if( flatValue > USHRT_MAX ) {
			raw_image[i] = USHRT_MAX;
		} else {
			raw_image[i] = flatValue;
		}
	}
}

void RawImage::ApplyDark( cli::array<unsigned short>^ pixels )
{
	for( int i = 0; i < raw_count; i++ ) {
		int value = raw_image[i];
		value -= pixels[i];
		value += offset;
		if( value < 0 ) {
			value = 0;
		} 
		raw_image[i] = value;
	}
}

void RawBuffer32::Add( RawImage^ rawImage )
{
	if( buffer == 0 ) {
		width = rawImage->Width;
		height = rawImage->Height;
		buffer = new unsigned int[width * height];
		memset( buffer, 0, width * height * sizeof( unsigned int ) );
	}

	const unsigned short* raw_image = rawImage->_raw_image();
	for( int i = 0; i < width * height; i++ ) {
		buffer[i] += raw_image[i];
	}

	count++;
}

RawBuffer32::~RawBuffer32()
{
	delete buffer;
	delete result;
}

void RawBuffer32::Add2( RawImage^ rawImage )
{
	margin = 7;
    int window = 2 * margin + 1;

	if( buffer == 0 ) {
		width = rawImage->Width;
		height = rawImage->Height;
		buffer = new unsigned int[width * height];
		memset( buffer, 0, width * height * sizeof( unsigned int ) );
		result = new unsigned short[width * height * window];
		sumSqr = new float[width * height];
		memset( sumSqr, 0, width * height * sizeof( float ) );
	}

	const unsigned short* raw_image = rawImage->_raw_image();

	if( count < window ) {
        for( int i = 0; i < width * height; i++ ) {
            int i0 = window * i;
            result[i0 + count] = raw_image[i];
            if( count + 1 == window ) {
				std::sort( result + i0, result + i0 + window);
				unsigned short p = result[i0 + margin];
				buffer[i] = p;
                sumSqr[i] = p * p + 0.5f;
            }
        }
    } else {
        for( int i = 0; i < width * height; i++ ) {
            int i0 = window * i;
            int center = i0 + margin;
            unsigned short p = raw_image[i];

            if( p > result[center - 1] && p < result[center + 1] ) {
                buffer[i] += p;
                sumSqr[i] += p * p;
            } else {
                int insertionPoint = i0;
                for( int j = i0; j < i0 + window; j++ ) {
                    if( j == center ) {
                        continue;
                    }
                    if( p <= result[j] ) {
                        if( j < center ) {
                            insertionPoint = j;
                        }
                        break;
                    }
                    insertionPoint = j;
                }

                if( insertionPoint < center ) {
                    unsigned short _p = result[center - 1];
                    buffer[i] += _p;
                    sumSqr[i] += _p * _p;
                    for( int j = center - 1; j > insertionPoint; j-- ) {
                        result[j] = result[j - 1];
                    }
                } else {
                    unsigned short  _p = result[center + 1];
                    buffer[i] += _p;
                    sumSqr[i] += _p * _p;
                    for( int j = center + 1; j < insertionPoint; j++ ) {
                        result[j] = result[j + 1];
                    }
                }
                result[insertionPoint] = p;
            }
        }
    }

	count++;
}

RawImage^ RawBuffer32::GetResult()
{
	// (!!!) ÓÒÅ×ÊÈ
	if( result == 0 ) {
		unsigned short* raw_image = new unsigned short[width * height];
		for( int i = 0; i < width * height; i++ ) {
			raw_image[i] = buffer[i] / count;
		}
		return gcnew RawImage( width, height, 0, raw_image );
	} else {
		unsigned int* skipped = new unsigned int[width * height];
		memset( skipped, 0, width * height * sizeof( unsigned short ) );
		unsigned char* skippedCount = new unsigned char[width * height];
		memset( skippedCount, 0, width * height * sizeof( unsigned char ) );

		unsigned short* raw_image = new unsigned short[width * height];
		int sumCount = count - 2 * margin;
		int window = 2 * margin + 1;
		for( int i = 0; i < width * height; i++ ) {
			int i0 = window * i;
            int center = i0 + margin;
			int sum = buffer[i];
            float mean = (float)sum / sumCount;
            double sigma = sqrt( sumSqr[i] / sumCount - mean * mean );
            int min = (int)( mean - 3 * sigma + 0.5f );
            int max = (int)( mean + 3 * sigma + 0.5f );
            int _count = sumCount;
            for( int j = i0; j < i0 + window; j++ ) {
                if( j != center ) {
                    unsigned short p = result[j];
                    if( p >= min && p <= max ) {
                        sum += p;
                        _count++;
                    } else {
						skipped[i] += p;
                        skippedCount[i]++;
                    }
                }
            }
            sum -= 512 *_count;
            sum /= _count;
            raw_image[i]= (ushort)( sum + 512 );
		}
		unsigned short* _skipped = new unsigned short[width * height];
		for( int i = 0; i < width * height; i++ ) {
			int c = skippedCount[i];
			if( c > 0 ) {
				_skipped[i] = (unsigned short)( skipped[i] / c );
			}
		}
		return gcnew RawImage( width, height, 0, raw_image );
	}
}

} //namespace LibRaw
} // namespace AstroPhoto
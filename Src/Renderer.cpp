// (!) CLR support switched off for the whole file. Approximately 4 times faster than managed version.
// (!) Using "#pragma unmanaged" showed unreliable results

#include <math.h>

#include <libraw.h>

#include <Renderer.h>

const int offset = 512;

Renderer::Renderer( unsigned short* _raw_image, int _raw_width, int _raw_height, unsigned int _idata_filters,
		unsigned char* _curveR, unsigned char* _curveG, unsigned char* _curveB )
{
	raw_width = _raw_width;
	raw_height = _raw_height;
	raw_image = _raw_image;
	idata_filters = _idata_filters;

	rgbPixels = 0;
	
	curveR = _curveR;
	curveG = _curveG;
	curveB = _curveB;
}

Renderer::Renderer( unsigned int* _rgbPixels, unsigned char* _curveR, unsigned char* _curveG, unsigned char* _curveB )
{
	raw_width = 0;
	raw_height = 0;
	raw_image = 0;
	idata_filters = 0;
	
	rgbPixels = _rgbPixels;
	
	curveR = _curveR;
	curveG = _curveG;
	curveB = _curveB;
}

void Renderer::RenderBitmapHalfRes( unsigned char* pixels, int stride, int saturation )
{
	if( saturation == 0 ) {
		renderBitmapHalfRes( pixels, stride );
	} else {
		renderBitmapHalfResSaturation( pixels, stride, saturation );
	}
}

void Renderer::renderBitmapHalfRes( unsigned char* pixels, int stride )
{
	int width = raw_width / 2;
	int height = raw_height / 2;

	for( int y = 0; y < height; y++ ) {
		int i0 = 2 * y * 2 * width;
		int j0 = y * stride;
		for( int x = 0; x < width; x++ ) {
			int i = i0 + 2 * x;
			int _i = i + raw_width;
			int j = j0 + 3 * x;

			pixels[j + 0] = curveB[raw_image[_i + 1]];
			pixels[j + 1] = curveG[( raw_image[i + 1] + raw_image[_i] ) / 2];
			pixels[j + 2] = curveR[raw_image[i]];
		}
	}
}

void Renderer::renderBitmapHalfResSaturation( unsigned char* pixels, int stride, int saturation )
{
	int width = raw_width / 2;
	int height = raw_height / 2;

	for( int y = 0; y < height; y++ ) {
		int i0 = 2 * y * 2 * width;
		int j0 = y * stride;
		for( int x = 0; x < width; x++ ) {
			int i = i0 + 2 * x;
			int _i = i + raw_width;
			int j = j0 + 3 * x;

			int R = curveR[raw_image[i]];
			int G = curveG[( raw_image[i + 1] + raw_image[_i] ) / 2];
			int B = curveB[raw_image[_i + 1]];

			int V = ( 10000 * ( R + G + B ) ) / 173;

			R += ( 100 * R - V ) / saturation;
			G += ( 100 * G - V ) / saturation;
			B += ( 100 * B - V ) / saturation;

			R = R > 255 ? 255 : ( R > 0 ? R : 0 );
			G = G > 255 ? 255 : ( G > 0 ? G : 0 );
			B = B > 255 ? 255 : ( B > 0 ? B : 0 );

			pixels[j + 0] = B;
			pixels[j + 1] = G;
			pixels[j + 2] = R;
		}
	}
}

void Renderer::RenderBitmap( unsigned char* pixels, int stride, const RECT& rect, int saturation )
{
	if( raw_image != 0 ) {
		rgbPixels = CalculateRgbPixelValues( rect );
	}

	if( saturation == 0 ) {
		renderBitmap( pixels, stride, rect.right - rect.left, rect.bottom - rect.top );
	} else {
		renderBitmapSaturation( pixels, stride, rect.right - rect.left, rect.bottom - rect.top, saturation );
	}
	
	if( raw_image != 0 ) {
		delete[] rgbPixels;
		rgbPixels = 0;
	}
}

void Renderer::renderBitmap( unsigned char* pixels, int stride, int rectWidth, int rectHeight )
{
	for( int y = 0; y < rectHeight; y++ ) {
		int i0 = 3 * y * rectWidth;
		int j0 = y * stride;
		for( int x = 0; x < rectWidth; x++ ) {
			int _x = 3 * x;
			int i = i0 + _x;
			int j = j0 + _x;
			pixels[j + 0] = curveB[rgbPixels[i + 2] + offset];
			pixels[j + 1] = curveG[rgbPixels[i + 1] + offset];
			pixels[j + 2] = curveR[rgbPixels[i + 0] + offset];
		}
	}
}

void Renderer::renderBitmapSaturation( unsigned char* pixels, int stride, int rectWidth, int rectHeight, int saturation )
{
	for( int y = 0; y < rectHeight; y++ ) {
		int i0 = 3 * y * rectWidth;
		int j0 = y * stride;
		for( int x = 0; x < rectWidth; x++ ) {
			int _x = 3 * x;
			int i = i0 + _x;
			int j = j0 + _x;

			int R = curveR[rgbPixels[i + 0] + offset];
			int G = curveG[rgbPixels[i + 1] + offset];
			int B = curveB[rgbPixels[i + 2] + offset];

			int V = ( 10000 * ( R + G + B ) ) / 173;

			R += ( 100 * R - V ) / saturation;
			G += ( 100 * G - V ) / saturation;
			B += ( 100 * B - V ) / saturation;

			R = R > 255 ? 255 : ( R > 0 ? R : 0 );
			G = G > 255 ? 255 : ( G > 0 ? G : 0 );
			B = B > 255 ? 255 : ( B > 0 ? B : 0 );

			pixels[j + 0] = B;
			pixels[j + 1] = G;
			pixels[j + 2] = R;
		}
	}
}

unsigned int* Renderer::CalculateRgbPixelValues( const RECT& rect )
{
	unsigned short* _raw_image = new unsigned short[raw_width * raw_height];
	
	copySubtractOffset( _raw_image, rect );
	filterSpikes( _raw_image, rect );
	unsigned int* rgbPixels = calculateRgbPixelValues( _raw_image, rect );
	
	delete[] _raw_image;
	
	return rgbPixels;
}

unsigned short* Renderer::ExtractFiltered( const RECT& rect )
{
	unsigned short* _raw_image = new unsigned short[raw_width * raw_height];

	copySubtractOffset( _raw_image, rect );
	filterSpikes( _raw_image, rect );

	return _raw_image;
}

void Renderer::copySubtractOffset( unsigned short* raw_image, const RECT& rect )
{
	int rectX = rect.left - 2;
	int rectY = rect.top - 2;
	int rectRight = rect.right + 2;
	int rectBottom = rect.bottom + 2;
	
	for( int y = rectY; y < rectBottom; y++ ) {
		int stride = y * raw_width; 
		for( int x = rectX; x < rectRight; x++ ) {
			int I = stride + x;
			int V = this->raw_image[I];
			raw_image[I] = V > offset ? V - offset : 0;
		}
	}
}

void Renderer::filterSpikes( unsigned short* raw_image, const RECT& rect )
{
	// Filter spikes

	int rectX = rect.left;
	int rectY = rect.top;
	int rectWidth = rect.right - rect.left;
	int rectHeight = rect.bottom - rect.top;

	for( int y = 0; y < rectHeight; y++ ) {
		for( int x = 0; x < rectWidth; x++ ) {
			int X = rectX + x ;
			int Y = rectY + y;
			int I = Y * raw_width + X;
			int cindex = ( idata_filters >> (((Y << 1 & 14) | (X & 1)) << 1) & 3);
			unsigned short C;
			switch( cindex ) {
				// R, B
				case 0:
				case 2:
					C = raw_image[I - 2] + raw_image[I + 2] +
						raw_image[I - 2 * raw_width] + raw_image[I + 2 * raw_width];
					if( raw_image[I] > 2 * C ) {
						//unsigned G = raw_image[I - 2] + raw_image[I + 2] + 
						//	raw_image[I - raw_width] + raw_image[I + raw_width];
						//if( C > G / 2 ) {
							raw_image[I] = C / 4;
						//}
					}
					break;
				// G1, G2
				case 1:
				case 3:
					C = raw_image[I - raw_width - 1] + raw_image[I - raw_width + 1] +
						raw_image[I + raw_width - 1] + raw_image[I + raw_width + 1];
					if( raw_image[I] > C ) {
						//unsigned C2 = raw_image[I - 2] + raw_image[I + 2] + 
						//	raw_image[I - raw_width] + raw_image[I + raw_width];
						//if( C > C2 / 2 ) {
							raw_image[I] = C / 4;
						//}
					}
					break;
			}
		}
	}
}

unsigned* Renderer::calculateRgbPixelValues( unsigned short* raw_image, const RECT& rect )
{
	int rectX = rect.left;
	int rectY = rect.top;
	int rectWidth = rect.right - rect.left;
	int rectHeight = rect.bottom - rect.top;
	
	// Calculate color map

	float* Cr = new float[rectWidth * rectHeight];
	float* Cg = new float[rectWidth * rectHeight];
	float* Cb = new float[rectWidth * rectHeight];
	
	for( int y = 0; y < rectHeight; y++ ) {
		for( int x = 0; x < rectWidth; x++ ) {
			int X = rectX + x;
			int Y = rectY + y;
			int i = y * rectWidth + x;
			int I = Y * raw_width + X;
			int cindex = ( idata_filters >> (((Y << 1 & 14) | (X & 1)) << 1) & 3);
			float R, G, B;
			switch( cindex ) {
				// R
				case 0:
					R = ( raw_image[I] + 
						raw_image[I - 2] + raw_image[I + 2] + 
						raw_image[I - 2 * raw_width] + raw_image[I + 2 * raw_width] ) / 5.0f;
					G = ( raw_image[I - 1] + raw_image[I + 1] +
						raw_image[I + raw_width] + raw_image[I - raw_width] ) / 4.0f;
					B = ( raw_image[I - raw_width - 1] + raw_image[I - raw_width + 1] + 
						raw_image[I + raw_width - 1] + raw_image[I + raw_width + 1] ) / 4.0f;
					break;
				// G1: RGR
				case 1: 
					G = ( raw_image[I] + 
						raw_image[I - raw_width - 1] + raw_image[I - raw_width + 1] +
						raw_image[I + raw_width - 1] + raw_image[I + raw_width + 1] ) / 5.0f;
					R = ( raw_image[I - 1] + raw_image[I + 1] ) / 2.0f;
					B = ( raw_image[I + raw_width] + raw_image[I - raw_width] ) / 2.0f;
					break;
				// B
				case 2:
					B = ( raw_image[I] + 
						raw_image[I - 2] + raw_image[I + 2] + 
						raw_image[I - 2 * raw_width] + raw_image[I + 2 * raw_width] ) / 5.0f;
					G = ( raw_image[I - 1] + raw_image[I + 1] +
						raw_image[I + raw_width] + raw_image[I - raw_width] ) / 4.0f;
					R = ( raw_image[I - raw_width - 1] + raw_image[I - raw_width + 1] + 
						raw_image[I + raw_width - 1] + raw_image[I + raw_width + 1] ) / 4.0f;
					break;
				// G2: BGB
				case 3: 
					G = ( raw_image[I] + 
						raw_image[I - raw_width - 1] + raw_image[I - raw_width + 1] +
						raw_image[I + raw_width - 1] + raw_image[I + raw_width + 1] ) / 5.0f;
					B = ( raw_image[I - 1] + raw_image[I + 1] ) / 2.0f;
					R = ( raw_image[I + raw_width] + raw_image[I - raw_width] ) / 2.0f;
					break;
			}
			
			// Normalize
			float C = sqrt( R * R + G * G + B * B );
			if( C > 0 ) {
				Cr[i] = R / C;
				Cg[i] = G / C;
				Cb[i] = B / C; 
			} else {
				Cr[i] = 1.0;
				Cg[i] = 1.0;
				Cb[i] = 1.0;
			}
		}
	}

	// Calculate rgb pixel values

	unsigned* rgb = new unsigned int[3 * rectWidth * rectHeight];
	for( int y = 0; y < rectHeight; y++ ) {
		for( int x = 0; x < rectWidth; x++ ) {
			int X = rectX + x;
			int Y = rectY + y;
			int i = y * rectWidth + x;
			int I = Y * raw_width + X;
			unsigned R = 0 ;
			unsigned G = 0 ;
			unsigned B = 0 ;
			int cindex = ( idata_filters >> (((Y << 1 & 14) | (X & 1)) << 1) & 3);
			switch( cindex ) {
				// R
				case 0:
					if( Cr[i] > 0 ) {
						R = raw_image[I];
						G = (unsigned int) ( raw_image[I] * Cg[i] / Cr[i] );
						B = (unsigned int) ( raw_image[I] * Cb[i] / Cr[i] );
					} else {
						G = raw_image[I] / 3;
						R = G;
						B = G;
					}
					break;
				// G1, G2
				case 1:
				case 3:
					if( Cg[i] > 0 ) {
						G = raw_image[I];
						R = (unsigned int) ( raw_image[I] * Cr[i] / Cg[i] );
						B = (unsigned int) ( raw_image[I] * Cb[i] / Cg[i] );
					} else {
						G = raw_image[I] / 3;
						R = G;
						B = G;
					}
					break;
				// B
				case 2:
					if( Cb[i] > 0 ) {
						B = raw_image[I];
						R = (unsigned int) ( raw_image[I] * Cr[i] / Cb[i] );
						G = (unsigned int) ( raw_image[I] * Cg[i] / Cb[i] );
					} else {
						G = raw_image[I] / 3;
						R = G;
						B = G;
					}
					break;
			}

			int j = 3 * ( y * rectWidth + x );
			rgb[j + 0] = R;
			rgb[j + 1] = G;
			rgb[j + 2] = B;
		}
	}

	delete[] Cr;
	delete[] Cg;
	delete[] Cb;

	return rgb;
}

void Renderer::RenderCFA( unsigned char* pixels, int stride, const RECT& rect )
{
	int _x = rect.left;
	int _y = rect.top;
	int _width = rect.right - rect.left;
	int _height = rect.bottom - rect.top;

	for( int y = 0; y < _height ; y++ ) {
		int i0 = y * stride;
		for( int x = 0; x < _width; x++ ) {
			int X = x + _x;
			int Y = y + _y;
			int i = i0 + 3 * x;
			if(  Y  < 0 || Y >= raw_height || X < 0 || X >= raw_width ) {
				continue;
			}
			unsigned value = raw_image[ ( y + _y  ) * raw_width + ( x + _x  ) ];
			int cindex = ( idata_filters >> (((Y << 1 & 14) | (X & 1)) << 1) & 3);
			switch( cindex ) {
				case 0:
					pixels[i + 0] = 0;
					pixels[i + 1] = 0;
					pixels[i + 2] = curveR[value];
					break;
				case 1:
					pixels[i + 0] = 0;
					pixels[i + 1] = curveG[value];
					pixels[i + 2] = 0;
					break;
				 case 2:
					pixels[i + 0] = curveB[value];
					pixels[i + 1] = 0;
					pixels[i + 2] = 0;
					break;
				case 3:
					pixels[i + 0] = 0;
					pixels[i + 1] = curveG[value];
					pixels[i + 2] = 0;
					break;
			}
		}
	}
}

/*

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
	for( unsigned long int x = 0; x < hwidth; x++ ) {
		unsigned long count = buf[x];
		values[x] = 231.4079 * ( log( count /10.0 + 10.0 ) - 1 );
		if( count > maxBuf ) {
			maxBuf = count;
		}
	}

	unsigned long hheight = 231.4079 * ( log( maxBuf/10.0 + 10.0 ) - 1 );

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
}*/
#include <math.h>

#include <Curves.h>

#using "mscorlib.dll"

using namespace System;
using namespace System::Drawing;
using namespace System::Runtime::InteropServices;

namespace AstroPhoto {
namespace LibRaw {

void gamma_curve( unsigned char *curve, int imin, int imax )
{
    double pwr = 1.0/2.2;
    double ts = 0.0;
    int size = 4096;
    int mode = 2;
	int i;
	double g[6], bnd[2]={0,0}, r;

	g[0] = pwr;
	g[1] = ts;
	g[2] = g[3] = g[4] = 0;
	bnd[g[1] >= 1] = 1;
	if (g[1] && (g[1]-1)*(g[0]-1) <= 0) {
		for (i=0; i < 48; i++) {
			g[2] = (bnd[0] + bnd[1])/2;
			if (g[0]) bnd[(pow(g[2]/g[1],-g[0]) - 1)/g[0] - 1/g[2] > -1] = g[2];
			else	bnd[g[2]/exp(1-1/g[2]) < g[1]] = g[2];
		}
		g[3] = g[2] / g[1];
		if (g[0]) g[4] = g[2] * (1/g[0] - 1);
	}
	if (g[0]) g[5] = 1 / (g[1]*(g[3]*g[3])/2 - g[4]*(1 - g[3]) +
		(1 - pow(g[3],1+g[0]))*(1 + g[4])/(1 + g[0])) - 1;
	else      g[5] = 1 / (g[1]*(g[3]*g[3])/2 + 1
		- g[2] - g[3] -	g[2]*g[3]*(log(g[3]) - 1)) - 1;
	for (i=0; i < size; i++) {
		if( i < imin ) {
			curve[i] = 0x0;
		} else if ( (r = (double) ( i - imin ) / imax ) < 1 ) {
			curve[i] = 0xFF * ( mode
			? (r < g[3] ? r*g[1] : (g[0] ? pow( r,g[0])*(1+g[4])-g[4]    : log(r)*g[2]+1))
			: (r < g[2] ? r/g[1] : (g[0] ? pow((r+g[4])/(1+g[4]),1/g[0]) : exp((r-1)/g[2]))));
		} else {
			curve[i] = 0xff;
		}
	}
}

Curve::~Curve()
{
	this->!Curve();
	GC::SuppressFinalize( this );
}

Curve::!Curve()
{
	delete curveR;
	curveR = 0;
	delete curveG;
	curveG = 0;
	delete curveB;
	curveB = 0;
}

public ref class LinearCurve : public Curve {
public:
	// 255 / 135 / 205
	LinearCurve() : maxV( 1024 ), Bg( 0 ), Kr( 255 ), Kg( 135 ), Kb( 245 ), BgR( 0 ), BgG( 0 ), BgB( 0 ) {}

	void SetBg( int newValue ) { Bg = newValue; invalidate(); }
	void SetBgRGB( int r, int g, int b ) { BgR = r; BgG = g; BgB = b; invalidate(); }
	void SetMaxV( int newValue ) { maxV = newValue; invalidate(); }
	void SetKg( int newValue ) { Kg = newValue; invalidate(); }
	void SetKb( int newValue ) { Kb = newValue; invalidate(); }

	void SetGamma( int newValue ) { gamma = newValue; invalidate(); }

protected:
	virtual void generateCurveR( unsigned char* curveR, int length ) override
	{
		init_curve( curveR, length, offset + ( ( BgR + Bg ) )/* 255 ) / Kr*/, Kr );
	}
	virtual void generateCurveG( unsigned char* curveG, int length ) override
	{
		init_curve( curveG, length, offset + ( ( BgG + Bg ) )/* 255 ) / Kg*/, Kg );
	}
	virtual void generateCurveB( unsigned char* curveB, int length ) override
	{
		init_curve( curveB, length, offset + ( ( BgB + Bg ) )/* 255 ) / Kb*/, Kb );
	}

	void init_curve( unsigned char * curve, int length, unsigned short offset, int K )
	{
		if( gamma == 0 ) {
			for( int i = 0; i < length; i++ ) {
				 int value = i > offset ? ( K * ( i - offset ) ) / maxV : 0 ;
				 curve[i] = value > 255 ? 255 : value;
			}
		} else {
			double k = ( (double)K ) / maxV;
			double g = ( ( (double) gamma * k ) / 255 / 10 );
			double kg = k / g;
			for( int i = 0; i < length; i++ ) {
				int x = i - offset;
				if( x > 0 ) {
					double value = kg * log( 1 + g * x ) + 0.5;
					curve[i] = value > 255 ? 255 : (int)value;
				} else {
					curve[i] = 0;
				}
			}
		}
	}

private:
	int Kr;
	int Kg;
	int Kb;

	int Bg;
	int BgR;
	int BgG;
	int BgB;
	int maxV;
	int gamma;
};

} //namespace LibRaw
} // namespace AstroPhoto
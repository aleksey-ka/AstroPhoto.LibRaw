#pragma once

namespace AstroPhoto {
namespace LibRaw {

const unsigned offset = 512;

public ref class Curve abstract {
public:
	Curve() : curveR( 0 ), curveG( 0 ), curveB( 0 ){}
	
	~Curve()
	{
		delete curveR;
		delete curveG;
		delete curveB;
	}

	unsigned char* R() 
	{ 
		if( curveR == 0 ) {
			curveR = new unsigned char[0x500000];
			generateCurveR( curveR, 0x500000 );
		}
		return curveR; 
	}

	unsigned char* G() 
	{ 
		if( curveG == 0 ) {
			curveG = new unsigned char[0x500000];
			generateCurveG( curveG, 0x500000 );
		}
		return curveG; 
	}

	unsigned char* B() 
	{ 
		if( curveB == 0 ) {
			curveB = new unsigned char[0x500000];
			generateCurveB( curveB, 0x500000 );
		}
		return curveB; 
	}

protected:
	virtual void generateCurveR( unsigned char* curveR, int length ) = 0;
	virtual void generateCurveG( unsigned char* curveG, int length ) = 0;
	virtual void generateCurveB( unsigned char* curveB, int length ) = 0;

	void invalidate()
	{
		delete curveR; 
		curveR = 0;
		delete curveG; 
		curveG = 0;
		delete curveB; 
		curveB = 0;
	}

private:
	unsigned char* curveR;
	unsigned char* curveG;
	unsigned char* curveB;
};

} //namespace LibRaw
} // namespace AstroPhoto
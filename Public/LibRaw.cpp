#include <atlstr.h>

#include <libraw.h>

#using "mscorlib.dll"

using namespace System;
using namespace System::Drawing;
using namespace System::Runtime::InteropServices;

#include <RawImage.h>

namespace AstroPhoto {
namespace LibRaw {

public ref class Instance {
public: 
	Instance() : data( libraw_init( LIBRAW_OPTIONS_NONE ) )
	{
		if( data == 0 ) {
			throw gcnew Exception( "Could not initialize LibRaw" );
		}
	}

	~Instance()
	{
		if( data != nullptr ) {
			libraw_recycle( data );
			libraw_close( data );
		}
		GC::SuppressFinalize( this );
	}

	property String^ version
	{
		String^ get()
		{ 
			return gcnew String( libraw_version() );
		}
	}

	RawImage^ load_raw( String^ filePath )
	{
		try {
			checkResult( libraw_open_wfile( data, CStringW( filePath ) ) );
			checkResult( libraw_unpack( data ) );
			return gcnew RawImage( data );
		} finally {
			libraw_recycle( data );
		}
	}

	RawImage^ load_raw_thumbnail( String^ filePath )
	{
		try {
			checkResult( libraw_open_wfile( data, CStringW( filePath ) ) );
			checkResult( libraw_unpack_thumb( data ) );
			checkResult( libraw_unpack( data ) );
			return gcnew RawImage( data );
		} finally {
			libraw_recycle( data );
		}
	}

	RawImage^ load_thumbnail( String^ filePath )
	{
		try {
			checkResult( libraw_open_wfile( data, CStringW( filePath ) ) );
			checkResult( libraw_unpack_thumb( data ) );
			return gcnew RawImage( data );
		} finally {
			libraw_recycle( data );
		}
	}

private:
	libraw_data_t* data;
	void checkResult( int code )
	{
		if( code != LIBRAW_SUCCESS ) {
			throw gcnew Exception( gcnew String( libraw_strerror( code ) ) );
		}
	}
};

} //namespace LibRaw
} // namespace AstroPhoto
#include "pvrdecoder.hpp"
#include "../squish/squish.h"

#include "../pvrlib/PVRTDecompress.h"

// 67B4FB3B-2125-5B3C-A99E-EAA01B6CD235
const GUID CLSID_PVR_Container = 
{ 0x67B4FB3B, 0x2125, 0x5B3C, { 0xA9, 0x9E, 0xEA, 0xA0, 0x1B, 0x6C, 0xD2, 0x35 } };

// EBF7CAF8-290F-568E-997A-7131AE4ECB1B
const GUID CLSID_PVR_Decoder = 
{ 0xEBF7CAF8, 0x290F, 0x568E, { 0x99, 0x7A, 0x71, 0x31, 0xAE, 0x4E, 0xCB, 0x1B} };

namespace pvrx
{
	template< typename T >
	static HRESULT ReadFromIStream( IStream *stream, T &val )
	{
		HRESULT result = S_OK;
		if ( NULL == stream ) result = E_INVALIDARG;
		ULONG numRead = 0;
		if ( SUCCEEDED( result )) result = stream->Read( &val, sizeof(T), &numRead );
		if ( SUCCEEDED( result )) result = ( sizeof(T) == numRead ) ? S_OK : E_UNEXPECTED;
		return result;
	}

	static HRESULT ReadBytesFromIStream( IStream *stream, unsigned char *bytes, unsigned count )
	{
		HRESULT result = S_OK;
		if ( NULL == stream ) result = E_INVALIDARG;
		ULONG numRead = 0;
		if ( SUCCEEDED( result )) result = stream->Read( bytes, count, &numRead );
		if ( SUCCEEDED( result )) result = ( count == numRead ) ? S_OK : E_UNEXPECTED;
		return result;
	}

	//----------------------------------------------------------------------------------------
	// PVR_FrameDecode implementation
	//----------------------------------------------------------------------------------------

	PVR_FrameDecode::PVR_FrameDecode( IWICImagingFactory *pIFactory, UINT num )
		: BaseFrameDecode( pIFactory, num )
	{
	}

	HRESULT PVR_FrameDecode::LoadImage( PVR_Texture_Header &pvrHeader, IStream *pIStream )
	{
		HRESULT result = S_OK;

		int width = pvrHeader.dwWidth;
		int height = pvrHeader.dwHeight;

		std::vector<unsigned char> data, uncompressed;

		uncompressed.resize(width * height * 4);

		int compressed_size;
		if ((pvrHeader.dwpfFlags & PVRTEX_PIXELTYPE) == OGL_PVRTC2)
			compressed_size = (std::max((unsigned int)width, PVRTC2_MIN_TEXWIDTH) * std::max((unsigned int)height, PVRTC2_MIN_TEXHEIGHT) * pvrHeader.dwBitCount + 7) / 8;
		else
			compressed_size = (std::max((unsigned int)width, PVRTC4_MIN_TEXWIDTH) * std::max((unsigned int)height, PVRTC4_MIN_TEXHEIGHT) * pvrHeader.dwBitCount + 7) / 8;

		data.resize( compressed_size );
		result = ReadBytesFromIStream( pIStream, &*data.begin(), compressed_size );

		if ( SUCCEEDED( result ))
		{
			if ((pvrHeader.dwpfFlags & PVRTEX_PIXELTYPE) == OGL_PVRTC2)
				PVRTCDecompress( &*data.begin(), 1, width, height, &*uncompressed.begin() );
			else
				PVRTCDecompress( &*data.begin(), 0, width, height, &*uncompressed.begin() );

			// vertical mirror
			unsigned *vdata = (unsigned*) &*uncompressed.begin();
			for ( int y = 0; y < height/2; ++y )
				for ( int x = 0; x < width; ++x )
				{
					std::swap( vdata[y*width+x], vdata[(height-y-1)*width+x] );
				}

			result = FillBitmapSource( width, height, 72, 72, GUID_WICPixelFormat32bpp3ChannelsAlpha, width*4,
				width*4*height, &*uncompressed.begin() );
		}

		return result;
	}

	HRESULT PVR_FrameDecode::FillBitmapSource( UINT width, UINT height, UINT dpiX, UINT dpiY,
		REFWICPixelFormatGUID pixelFormat, UINT cbStride, UINT cbBufferSize, BYTE *pbBuffer )
	{
		UNREFERENCED_PARAMETER( dpiX );
		UNREFERENCED_PARAMETER( dpiY );

		HRESULT result = S_OK;

		if ( SUCCEEDED( result ))
			result = m_factory->CreateBitmapFromMemory ( width, height, pixelFormat, cbStride, cbBufferSize,
				pbBuffer, reinterpret_cast<IWICBitmap **>( &( m_bitmapSource )));

//		m_thumbnail = m_bitmapSource;

		return result;
	}

	//----------------------------------------------------------------------------------------
	// PVR_Decoder implementation
	//----------------------------------------------------------------------------------------

	PVR_Decoder::PVR_Decoder()
		: BaseDecoder( CLSID_PVR_Decoder, CLSID_PVR_Container )
	{
	}

	PVR_Decoder::~PVR_Decoder()
	{
	}

	// TODO: implement real query capability
	STDMETHODIMP PVR_Decoder::QueryCapability( IStream *pIStream, DWORD *pCapability )
	{
		UNREFERENCED_PARAMETER( pIStream );

		HRESULT result = S_OK;

		*pCapability =
			WICBitmapDecoderCapabilityCanDecodeAllImages;

		return result;
	}

	STDMETHODIMP PVR_Decoder::Initialize( IStream *pIStream, WICDecodeOptions cacheOptions )
	{
		UNREFERENCED_PARAMETER( cacheOptions );

		HRESULT result = E_INVALIDARG;

		ReleaseMembers( true );

		if ( pIStream )
		{
			PVR_Texture_Header pvrHeader;

			result = ReadFromIStream( pIStream, pvrHeader ); // read header

			if ( SUCCEEDED( result )) result = VerifyFactory();

			if ( SUCCEEDED( result ))
			{
				if (((pvrHeader.dwpfFlags & PVRTEX_PIXELTYPE) != OGL_PVRTC2) && ((pvrHeader.dwpfFlags & PVRTEX_PIXELTYPE) != OGL_PVRTC4))
				{
					result = E_INVALIDARG;
				}
				else
				{
					PVR_FrameDecode* frame = CreateNewDecoderFrame( m_factory, 0 ); 

					result = frame->LoadImage( pvrHeader, pIStream );
				
					if ( SUCCEEDED( result ))
						AddDecoderFrame( frame );
					else
						delete frame;
				}
			}
		}
		else
			result = E_INVALIDARG;

		return result;
	}

	PVR_FrameDecode* PVR_Decoder::CreateNewDecoderFrame( IWICImagingFactory* factory , UINT i )
	{
		return new PVR_FrameDecode( factory, i );
	}

	void PVR_Decoder::Register( RegMan &regMan )
	{
		HMODULE curModule = GetModuleHandle( L"dds-wic-codec.dll" );
		wchar_t tempFileName[MAX_PATH];
		if ( curModule != NULL ) GetModuleFileName( curModule, tempFileName, MAX_PATH );

		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"CLSID", L"{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}" );
		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"FriendlyName", L"PVR Decoder" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"Version", L"1.0.0.1" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"Date", _T(__DATE__) );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"SpecVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"ColorManagementVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"MimeTypes", L"x-image/pvr" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"FileExtensions", L".pvr" );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"SupportsAnimation", 0 );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"SupportChromakey", 1 );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"SupportLossless", 1 );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"SupportMultiframe", 1 );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"ContainerFormat", L"{67B4FB3B-2125-5B3C-A99E-EAA01B6CD235}" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"Author", L"Ilya Zaytsev, r2vb.com" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"Description", L"PVRTC Format Decoder" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}", L"FriendlyName", L"PVRTC Decoder" );

		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}\\Formats", L"", L"" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}\\Formats\\{6FDDC324-4E03-4BFE-B185-3D77768DC90F}", L"", L"" );

		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}\\InprocServer32", L"", tempFileName );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}\\InprocServer32", L"ThreadingModel", L"Apartment" );
		regMan.SetSZ( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}\\Patterns", L"", L"" );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}\\Patterns\\0", L"Position", 0 );
		regMan.SetDW( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}\\Patterns\\0", L"Length", 4 );

		BYTE bytes[8] = { 0 };
		bytes[0] = 0x34; bytes[1] = 0x0; bytes[2] = 0x0; bytes[3] = 0x0;
		regMan.SetBytes( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}\\Patterns\\0", L"Pattern", bytes, 4 );
		bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0xFF;
		regMan.SetBytes( L"CLSID\\{EBF7CAF8-290F-568E-997A-7131AE4ECB1B}\\Patterns\\0", L"Mask", bytes, 4 );
	}
}

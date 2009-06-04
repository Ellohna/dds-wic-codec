#pragma once

#include "../stdafx.hpp"
#include "../wicx/basedecoder.hpp"
#include "../wicx/regman.hpp"

#include "../pvrlib/PVRTTexture.h"

extern const GUID CLSID_PVR_Container;
extern const GUID CLSID_PVR_Decoder;

namespace pvrx
{
	using namespace wicx;

	class PVR_FrameDecode: public BaseFrameDecode
	{
	public:
		PVR_FrameDecode( IWICImagingFactory *pIFactory, UINT num );

		HRESULT LoadImage( PVR_Texture_Header &pvrHeader, IStream *pIStream );

	private:
		HRESULT FillBitmapSource( UINT width, UINT height, UINT dpiX, UINT dpiY, REFWICPixelFormatGUID pixelFormat,
			UINT cbStride, UINT cbBufferSize, BYTE *pbBuffer );
	};

	class PVR_Decoder: public BaseDecoder
	{
	public:
		static void Register( RegMan &regMan );

		PVR_Decoder();
		~PVR_Decoder();

		// IWICBitmapDecoder interface

		STDMETHOD( QueryCapability )( 
			/* [in] */ IStream *pIStream,
			/* [out] */ DWORD *pCapability );

		STDMETHOD( Initialize )( 
			/* [in] */ IStream *pIStream,
			/* [in] */ WICDecodeOptions cacheOptions );

	protected:
		virtual PVR_FrameDecode* CreateNewDecoderFrame( IWICImagingFactory *factory , UINT i );
	};
}

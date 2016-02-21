// vgmByteStreamHandler.cpp : Implementation of CvgmByteStreamHandler

#include "stdafx.h"
#include "vgmByteStreamHandler.h"

namespace{
	const size_t HEADERBUFF_LEN = 12;
}
// CvgmByteStreamHandler

IFACEMETHODIMP CvgmByteStreamHandler::BeginCreateObject(
	/* [in] */ IMFByteStream *pByteStream,
	/* [in] */ LPCWSTR pwszURL,
	/* [in] */ DWORD dwFlags,
	/* [in] */ IPropertyStore *pProps,
	/* [annotation][out] */
	_Outptr_opt_  IUnknown **ppIUnknownCancelCookie,
	/* [in] */ IMFAsyncCallback *pCallback,
	/* [in] */ IUnknown *punkState) _NOEXCEPT
{
	UNREFERENCED_PARAMETER(pProps);
	UNREFERENCED_PARAMETER(pwszURL);
	if ((pByteStream == nullptr) || (pCallback == nullptr))
	{
		return E_POINTER;
	}
	if ((dwFlags & MF_RESOLUTION_MEDIASOURCE) == 0)
	{
		return E_INVALIDARG;
	}

	if (ppIUnknownCancelCookie)
	{
		this->QueryInterface(IID_PPV_ARGS(ppIUnknownCancelCookie));
	}
	
	ATL::CComPtr<IMFByteStream> toOutput;

	HRESULT hr = pByteStream->SetCurrentPosition(0L);
	if (FAILED(hr))
	{
		return hr;
	}
	BYTE headerBuff[HEADERBUFF_LEN];
	ULONG read = 0;
	hr = pByteStream->Read(headerBuff, HEADERBUFF_LEN, std::addressof(read));
	if (FAILED(hr) || read < HEADERBUFF_LEN)
	{
		return MF_E_CANNOT_PARSE_BYTESTREAM;
	}

	bool isVGM = true;
	// verify the header starts with "Vgm " (space included) 32bits Little Endian
	if (headerBuff[0] != 'V' && headerBuff[1] != 'g' && headerBuff[2] != 'm' && headerBuff[3] != ' ')
	{
		isVGM = false;
	}

	// it's actually a VGZ file and is GZipped, we need to UnGzip
	if (headerBuff[0] == 0x1F && headerBuff[1] == 0x8B)
	{
		try
		{
			toOutput = this->handleGZip(pByteStream);
		}
		catch (_com_error & ex)
		{
			hr = ex.Error();
		}
		catch (...){
			hr = E_FAIL;
		}
	}
	else
	{
		toOutput = pByteStream;
	}


	ATL::CComPtr<IVGMSource> pSource;
	ATL::CComPtr<IMFAsyncResult> pResult;

	hr = pSource.CoCreateInstance(CLSID_vgmSource);

	if (SUCCEEDED(hr))
	{
		hr = pSource->Open(toOutput);
	}

	if (SUCCEEDED(hr))
	{
		hr = ::MFCreateAsyncResult(pSource, pCallback, punkState, &pResult);
	}
	if (SUCCEEDED(hr))
	{
		::MFInvokeCallback(pResult);
	}

	return hr;
}

IFACEMETHODIMP CvgmByteStreamHandler::EndCreateObject(
	/* [in] */ IMFAsyncResult *pResult,
	/* [annotation][out] */
	_Out_  MF_OBJECT_TYPE *pObjectType,
	/* [annotation][out] */
	_Outptr_  IUnknown **ppObject) _NOEXCEPT
{
	HRESULT hr = S_OK;

	if ((pResult == nullptr) || (ppObject == nullptr) || (ppObject == nullptr))
	{
		return E_INVALIDARG;
	}

	*pObjectType = MF_OBJECT_INVALID;
	
	ATL::CComPtr<IUnknown> pUnk;

	hr = pResult->GetObject(&pUnk);

	if (SUCCEEDED(hr))
	{
		ATL::CComPtr<IMFMediaSource> pSource;
		// Minimal sanity check - is it really a media source?
		hr = pUnk->QueryInterface(IID_PPV_ARGS(&pSource));
	}

	if (SUCCEEDED(hr))
	{
		pUnk->QueryInterface(IID_PPV_ARGS(ppObject));
		*pObjectType = MF_OBJECT_MEDIASOURCE;
	}

	return hr;
}

IFACEMETHODIMP CvgmByteStreamHandler::CancelObjectCreation(
	/* [in] */ IUnknown *pIUnknownCancelCookie) _NOEXCEPT
{
	UNREFERENCED_PARAMETER(pIUnknownCancelCookie);
	return E_NOTIMPL;
}

IFACEMETHODIMP CvgmByteStreamHandler::GetMaxNumberOfBytesRequiredForResolution(
	/* [annotation][out] */
	_Out_  QWORD *pqwBytes) _NOEXCEPT
{
	if (pqwBytes == nullptr)
	{
		return E_INVALIDARG;
	}

	// In a canonical VGM .vgm file, the start of the 'data' chunk is at byte offset 64.
	*pqwBytes = 64;
	return S_OK;
}

ATL::CComPtr<IMFByteStream> CvgmByteStreamHandler::handleGZip(gsl::not_null<IMFByteStream*> stream)
{
	QWORD streamLength = 0;
	_com_util::CheckError(stream->SetCurrentPosition(0));
	_com_util::CheckError(stream->GetLength(std::addressof(streamLength)));
	std::stringstream outBuff;
	ULONG read = 0;
		
	{
		std::vector<BYTE> buffer(streamLength);
		_com_util::CheckError(stream->Read(std::addressof(buffer[0]), streamLength, std::addressof(read)));
		boost::iostreams::filtering_istreambuf strBuff;
		strBuff.push(boost::iostreams::gzip_decompressor());
		strBuff.push(boost::iostreams::array_source(reinterpret_cast<char*>(std::addressof(buffer[0])), buffer.size()));
		boost::iostreams::copy(strBuff, outBuff);
	}
	
	ATL::CComPtr<IMFByteStream> decompressed;

	_com_util::CheckError(::MFCreateTempFile(
		MF_ACCESSMODE_READWRITE,
		MF_OPENMODE_DELETE_IF_EXIST,
		MF_FILEFLAGS_NONE,
		&decompressed));
	// get the length of the stream
	outBuff.seekg(0);
	outBuff.seekg(0, std::ios::end);
	size_t length = outBuff.tellg();
	{
		std::vector<BYTE> copyBuff(length);
		outBuff.seekg(0, std::ios::beg);
		outBuff.read(reinterpret_cast<char*>(std::addressof(copyBuff[0])), length);
		ULONG written = 0;
		_com_util::CheckError(decompressed->Write(std::addressof(copyBuff[0]), length, std::addressof(written)));
	}
	_com_util::CheckError(decompressed->SetCurrentPosition(0));
	return decompressed;
}
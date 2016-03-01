// vgmByteStreamHandler.cpp : Implementation of CvgmByteStreamHandler

#include "stdafx.h"
#include "vgmByteStreamHandler.h"
#include "ExceptionTranslator.h"

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
	/* [in] */ IUnknown *punkState) noexcept
{
	UNREFERENCED_PARAMETER(pProps);
	UNREFERENCED_PARAMETER(pwszURL);
	vgmcodec::exceptions::ExceptionTranslator trans{ 
		[pByteStream, pCallback, dwFlags, punkState, &ppIUnknownCancelCookie, this]() {
			vgmcodec::exceptions::check_pointer(pByteStream);
			vgmcodec::exceptions::check_pointer(pCallback);

			if ((dwFlags & MF_RESOLUTION_MEDIASOURCE) == 0)
			{
				_com_issue_error(E_INVALIDARG);
			}
			if (ppIUnknownCancelCookie)
			{
				_com_util::CheckError(this->QueryInterface(IID_PPV_ARGS(ppIUnknownCancelCookie)));
			}

			_com_util::CheckError(pByteStream->SetCurrentPosition(0L));

			std::array<BYTE, HEADERBUFF_LEN> headerBuff;
			ULONG read = 0;
			HRESULT hr = pByteStream->Read(headerBuff.data(), static_cast<ULONG>(headerBuff.size()), std::addressof(read));
			if (FAILED(hr) || read < headerBuff.size())
			{
				_com_issue_error(MF_E_CANNOT_PARSE_BYTESTREAM);
			}

			bool isVGM = true;
			// verify the header starts with "Vgm " (space included) 32bits Little Endian
			if (headerBuff[0] != 'V' && headerBuff[1] != 'g' && headerBuff[2] != 'm' && headerBuff[3] != ' ')
			{
				isVGM = false;
			}

			ATL::CComPtr<IMFByteStream> toOutput;
			// it's actually a VGZ file and is GZipped, we need to UnGzip
			if (headerBuff[0] == 0x1F && headerBuff[1] == 0x8B)
			{
				toOutput = this->handleGZip(pByteStream);
			}
			else
			{
				toOutput = pByteStream;
			}

			ATL::CComPtr<IVGMSource> pSource;
			_com_util::CheckError(pSource.CoCreateInstance(CLSID_vgmSource));
			_com_util::CheckError(pSource->Open(toOutput));
			ATL::CComPtr<IMFAsyncResult> pResult;
			_com_util::CheckError(::MFCreateAsyncResult(pSource, pCallback, punkState, &pResult));
			_com_util::CheckError(::MFInvokeCallback(pResult));
		}
	};
	return trans();
}

IFACEMETHODIMP CvgmByteStreamHandler::EndCreateObject(
	/* [in] */ IMFAsyncResult *pResult,
	/* [annotation][out] */
	_Out_  MF_OBJECT_TYPE *pObjectType,
	/* [annotation][out] */
	_Outptr_  IUnknown **ppObject) noexcept
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
		hr = pUnk.QueryInterface(&pSource);
	}

	if (SUCCEEDED(hr))
	{
		hr = pUnk.QueryInterface(ppObject);
		*pObjectType = MF_OBJECT_MEDIASOURCE;
	}

	return hr;
}

IFACEMETHODIMP CvgmByteStreamHandler::CancelObjectCreation(
	/* [in] */ IUnknown *pIUnknownCancelCookie) noexcept
{
	UNREFERENCED_PARAMETER(pIUnknownCancelCookie);
	return E_NOTIMPL;
}

IFACEMETHODIMP CvgmByteStreamHandler::GetMaxNumberOfBytesRequiredForResolution(
	/* [annotation][out] */
	_Out_  QWORD *pqwBytes) noexcept
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
	_com_util::CheckError(stream->SetCurrentPosition(0));
	QWORD streamLength = 0;
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
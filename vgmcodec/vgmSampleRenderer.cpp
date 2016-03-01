// vgmSampleRenderer.cpp : Implementation of CvgmSampleRenderer

#include "stdafx.h"
#include "vgmSampleRenderer.h"
#include "vgmheader.h"
#include "ChipFactory.h"
#include "lockHelper.h"
#include "bufferLocker.h"
#include "ExceptionTranslator.h"

namespace{
	std::int16_t inline LimitToShort(std::int32_t Value)
	{
		std::int32_t NewValue = Value;
		if (NewValue < -0x8000)
			NewValue = -0x8000;
		if (NewValue > 0x7FFF)
			NewValue = 0x7FFF;

		return static_cast<std::int16_t>(NewValue);
	}
}
// CvgmSampleRenderer
using namespace vgmcodec::transform;

CvgmSampleRenderer::CvgmSampleRenderer(void) noexcept
	:m_samplesPosition(0),
	m_samplesPlayed(0),
	m_outputQueue({}),
	m_queueLocation(m_outputQueue.begin())	
{
}


COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetStreamLimits( 
	/* [out] */ DWORD *pdwInputMinimum,
	/* [out] */ DWORD *pdwInputMaximum,
	/* [out] */ DWORD *pdwOutputMinimum,
	/* [out] */ DWORD *pdwOutputMaximum) noexcept
{
	if ((pdwInputMinimum == nullptr) ||
		(pdwInputMaximum == nullptr) ||
		(pdwOutputMinimum == nullptr) ||
		(pdwOutputMaximum == nullptr))
	{
		return E_POINTER;
	}


	// This MFT has a fixed number of streams.
	*pdwInputMinimum = 1;
	*pdwInputMaximum = 1;
	*pdwOutputMinimum = 1;
	*pdwOutputMaximum = 1;

	return S_OK;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetStreamCount( 
	/* [out] */ __RPC__out DWORD *pcInputStreams,
	/* [out] */ __RPC__out DWORD *pcOutputStreams)
{
	if (pcInputStreams == nullptr || pcOutputStreams == nullptr)

	{
		return E_POINTER;
	}

	// This MFT has a fixed number of streams.
	*pcInputStreams = 1;
	*pcOutputStreams = 1;

	return S_OK;
}

STDMETHODIMP CvgmSampleRenderer::GetInputStreamInfo( 
	DWORD dwInputStreamID,
	/* [out] */ __RPC__out MFT_INPUT_STREAM_INFO *pStreamInfo) noexcept
{
	if (pStreamInfo == nullptr)
	{
		return E_POINTER;
	}

	if (!IsValidInputStream(dwInputStreamID))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}

	pStreamInfo->hnsMaxLatency = 0;

	//  We can process data on any boundary.
	pStreamInfo->dwFlags = 0;

	pStreamInfo->cbSize = 1;
	pStreamInfo->cbMaxLookahead = 0;
	pStreamInfo->cbAlignment = 1;

	return S_OK;
}

STDMETHODIMP CvgmSampleRenderer::GetOutputStreamInfo( 
	DWORD dwOutputStreamID,
	/* [out] */ __RPC__out MFT_OUTPUT_STREAM_INFO *pStreamInfo) noexcept
{
	if (pStreamInfo == nullptr)
	{
		return E_POINTER;
	}

	if (!IsValidOutputStream(dwOutputStreamID))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}

	vgmcodec::helpers::lockHelper lock(*this);

	pStreamInfo->dwFlags = 
		MFT_OUTPUT_STREAM_WHOLE_SAMPLES;
	if (m_pOutputType == nullptr)
	{
		pStreamInfo->cbSize = 0;
		pStreamInfo->cbAlignment = 0;
	}
	else
	{
		pStreamInfo->cbSize = AVG_OUTPUT_BYTES_PER_SEC(VGM_SAMPLE_RATE);
		pStreamInfo->cbAlignment = OUTPUT_BLOCK_ALIGN;
	}

	//this->Unlock();

	return S_OK;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetOutputAvailableType( 
	DWORD dwOutputStreamID,
	DWORD dwTypeIndex,
	/* [out] */ __RPC__deref_out_opt IMFMediaType **ppType) noexcept
{
	if (ppType == nullptr)
	{
		return E_INVALIDARG;
	}

	if (!this->IsValidOutputStream(dwOutputStreamID))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}

	if (dwTypeIndex != 0)
	{
		return MF_E_NO_MORE_TYPES;
	}

	vgmcodec::exceptions::ExceptionTranslator trans{
		[this, dwOutputStreamID, dwTypeIndex, &ppType]() {
			vgmcodec::helpers::lockHelper lock(*this);
			if (m_pInputType == nullptr)
			{
				_com_issue_error(MF_E_TRANSFORM_TYPE_NOT_SET);
			}
			ATL::CComPtr<IMFMediaType> pOutputType;

			_com_util::CheckError(::MFCreateMediaType(&pOutputType));
			_com_util::CheckError(pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
			_com_util::CheckError(pOutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
			_com_util::CheckError(pOutputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));
			_com_util::CheckError(pOutputType->SetUINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, TRUE));
			_com_util::CheckError(pOutputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, CHANNEL_COUNT));
			static_assert(16 == BITS_PERSAMPLE, "invalid number of bits per sample");
			_com_util::CheckError(pOutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, BITS_PERSAMPLE));
			_com_util::CheckError(pOutputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, VGM_SAMPLE_RATE));
			static_assert(4 == OUTPUT_BLOCK_ALIGN, "invalid output block alignment");
			_com_util::CheckError(pOutputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, OUTPUT_BLOCK_ALIGN));
			_com_util::CheckError(pOutputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, AVG_OUTPUT_BYTES_PER_SEC(VGM_SAMPLE_RATE)));
			_com_util::CheckError(pOutputType.QueryInterface(ppType));
		}
	};

	return trans();
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetStreamIDs( 
	DWORD dwInputIDArraySize,
	/* [size_is][out] */ DWORD *pdwInputIDs,
	DWORD dwOutputIDArraySize,
	/* [size_is][out] */ DWORD *pdwOutputIDs) noexcept
{
	UNREFERENCED_PARAMETER(dwInputIDArraySize);
	UNREFERENCED_PARAMETER(pdwInputIDs);
	UNREFERENCED_PARAMETER(dwOutputIDArraySize);
	UNREFERENCED_PARAMETER(pdwOutputIDs);
	// Do not need to implement, because this MFT has a fixed number of 
	// streams and the stream IDs match the stream indexes.
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetAttributes( 
	/* [out] */ IMFAttributes **pAttributes) noexcept
{
	UNREFERENCED_PARAMETER(pAttributes);
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetInputStreamAttributes( 
	DWORD dwInputStreamID,
	/* [out] */ IMFAttributes **pAttributes) noexcept
{
	UNREFERENCED_PARAMETER(dwInputStreamID);
	UNREFERENCED_PARAMETER(pAttributes);
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetOutputStreamAttributes( 
	DWORD dwOutputStreamID,
	/* [out] */ IMFAttributes **pAttributes) noexcept
{
	UNREFERENCED_PARAMETER(dwOutputStreamID);
	UNREFERENCED_PARAMETER(pAttributes);
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::SetInputType( 
	DWORD dwInputStreamID,
	/* [in] */ IMFMediaType *pType,
	DWORD dwFlags) noexcept
{
	if (!this->IsValidInputStream(dwInputStreamID))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}

	// Validate flags.
	if (dwFlags & ~MFT_SET_TYPE_TEST_ONLY)
	{
		return E_INVALIDARG;
	}

	HRESULT hr = S_OK;
	vgmcodec::helpers::lockHelper lock(*this);

	// Does the caller want us to set the type, or just test it?
	bool bReallySet = ((dwFlags & MFT_SET_TYPE_TEST_ONLY) == 0);

	// If we have an input sample, the client cannot change the type now.
	if (this->HasPendingOutput())
	{
		hr = MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING;
	}

	// Validate the type, if non-NULL.
	if (SUCCEEDED(hr))
	{
		if (pType)
		{
			hr = OnCheckInputType(pType);
		}
	}

	if (SUCCEEDED(hr))
	{
		// The type is OK. 
		// Set the type, unless the caller was just testing.
		if (bReallySet)
		{
			m_pInputType.Release();

			this->m_pInputType = pType;
			hr = this->OnSetInputType(pType);
		}
	}

	//this->Unlock();
	return hr;
}

COM_DECLSPEC_NOTHROW HRESULT CvgmSampleRenderer::OnCheckInputType(IMFMediaType *pmt) noexcept
{
	//  Check if the type is already set and if so reject any type that's not identical.
	if (m_pInputType)
	{
		DWORD dwFlags = 0;
		if (S_OK == m_pInputType->IsEqual(pmt, &dwFlags))
		{
			return S_OK;
		}
		else
		{
			return MF_E_INVALIDTYPE;
		}
	}

	GUID majortype = { 0 };
	
	HRESULT hr = pmt->GetMajorType(std::addressof(majortype));

	if (SUCCEEDED(hr) && !IsEqualGUID(majortype, MFMediaType_Audio))
		hr = MF_E_INVALIDTYPE;

	GUID subtype = { 0 };
	if (SUCCEEDED(hr))
		hr = pmt->GetGUID(MF_MT_SUBTYPE, std::addressof(subtype));

	if (SUCCEEDED(hr) && !IsEqualGUID(subtype, MFAudioFormat_VGM))
		hr = MF_E_INVALIDTYPE;

	return hr;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::DeleteInputStream( 
	DWORD dwStreamID) noexcept
{
	UNREFERENCED_PARAMETER(dwStreamID);
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::AddInputStreams( 
	DWORD cStreams,
	/* [in] */ DWORD *adwStreamIDs) noexcept
{
	UNREFERENCED_PARAMETER(cStreams);
	UNREFERENCED_PARAMETER(adwStreamIDs);
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetInputAvailableType(
	DWORD dwInputStreamID,
	DWORD dwTypeIndex,
	/* [out] */ IMFMediaType **ppType) noexcept
{
	UNREFERENCED_PARAMETER(dwInputStreamID);
	UNREFERENCED_PARAMETER(dwTypeIndex);
	UNREFERENCED_PARAMETER(ppType);
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetInputStatus( 
	DWORD dwInputStreamID,
	/* [out] */  DWORD *pdwFlags) noexcept
{
	if (pdwFlags == nullptr)
	{
		return E_POINTER;
	}

	if (!IsValidInputStream(dwInputStreamID))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}
	vgmcodec::helpers::lockHelper lock(*this);

	// If we already have an input sample, we don't accept
	// another one until the client calls ProcessOutput or Flush.
	if (HasPendingOutput())
	{
		*pdwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;
	}
	else
	{
		*pdwFlags = 0;
	}

	return S_OK;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetOutputStatus( 
	/* [out] */  DWORD *pdwFlags) noexcept
{
	if (pdwFlags == nullptr)
	{
		return E_POINTER;
	}
	vgmcodec::helpers::lockHelper lock(*this);

	// We can produce an output sample if (and only if)
	// we have an input sample.
	if (HasPendingOutput())
	{
		*pdwFlags = MFT_OUTPUT_STATUS_SAMPLE_READY;
	}
	else
	{
		*pdwFlags = 0;
	}

	return S_OK;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::ProcessEvent( 
	DWORD dwInputStreamID,
	/* [in] */  IMFMediaEvent *pEvent) noexcept
{
	UNREFERENCED_PARAMETER(dwInputStreamID);
	UNREFERENCED_PARAMETER(pEvent);
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::SetOutputType( 
	DWORD dwOutputStreamID,
	/* [in] */ IMFMediaType *pType,
	DWORD dwFlags) noexcept
{
	if (!IsValidOutputStream(dwOutputStreamID))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}

	// Validate flags.
	if (dwFlags & ~MFT_SET_TYPE_TEST_ONLY)
	{
		return E_INVALIDARG;
	}

	HRESULT hr = S_OK;
	vgmcodec::helpers::lockHelper lock(*this);

	// Does the caller want us to set the type, or just test it?
	bool bReallySet = ((dwFlags & MFT_SET_TYPE_TEST_ONLY) == 0);

	// If we have an input sample, the client cannot change the type now.
	if (HasPendingOutput())
	{
		hr = MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING;
	}

	// Validate the type, if non-NULL.
	if (SUCCEEDED(hr))
	{
		if (pType)
		{
			hr = OnCheckOutputType(pType);
		}
	}

	if (SUCCEEDED(hr))
	{
		if (bReallySet)
		{
			// The type is OK. 
			// Set the type, unless the caller was just testing.
			hr = OnSetOutputType(pType);
		}
	}
	return hr;
}

HRESULT CvgmSampleRenderer::OnSetInputType(IMFMediaType * pmt) noexcept
{
	try{
		UINT32 headerSize = 0;
		_com_util::CheckError(pmt->GetBlobSize(MF_MT_VGM_HEADER, std::addressof(headerSize)));

		if (sizeof(vgmcodec::format::vgmHeader) != headerSize){
			return E_INVALIDARG;
		}
		vgmcodec::format::vgmHeader header = { 0 };
		_com_util::CheckError(
			pmt->GetBlob(
				MF_MT_VGM_HEADER,
				reinterpret_cast<UINT8*>(
					std::addressof(header)),
				sizeof(vgmcodec::format::vgmHeader),
				std::addressof(headerSize)));
		std::unique_ptr<vgmcodec::transform::CChipFactory> factory =
			CChipFactory::createFactory(header);
		this->m_vChips = factory->getChips();
	}
	catch (std::invalid_argument const& ex){
		UNREFERENCED_PARAMETER(ex);
		return E_INVALIDARG;
	}
	catch (_com_error const& ex){
		return ex.Error();
	}
	catch (...){
		return E_FAIL;
	}
	
	return S_OK;
}

HRESULT CvgmSampleRenderer::OnCheckOutputType(IMFMediaType *pmt) noexcept
{
	//  Check if the type is already set and if so reject any type that's not identical.
	if (m_pOutputType)
	{
		DWORD dwFlags = 0;
		if (S_OK == m_pOutputType->IsEqual(pmt, &dwFlags))
		{
			return S_OK;
		}
		else
		{
			return MF_E_INVALIDTYPE;
		}
	}

	if (m_pInputType == nullptr)
	{
		return MF_E_TRANSFORM_TYPE_NOT_SET; // Input type must be set first.
	}


	HRESULT hr = S_OK;
	BOOL bMatch = FALSE;

	ATL::CComPtr<IMFMediaType> pOurType;

	// Make sure their type is a superset of our proposed output type.
	hr = this->GetOutputAvailableType(0, 0, &pOurType);

	if (SUCCEEDED(hr))
	{
		hr = pOurType->Compare(pmt, MF_ATTRIBUTES_MATCH_OUR_ITEMS, std::addressof(bMatch));
	}

	if (SUCCEEDED(hr))
	{
		if (!bMatch)
		{
			hr = MF_E_INVALIDTYPE;
		}
	}
	_RPTFW1(_CRT_WARN, L"CodecBase::OnCheckOutputType HRESULT: %X\n", hr);
	return hr;
}

HRESULT CvgmSampleRenderer::OnSetOutputType(IMFMediaType * pmt) noexcept
{

	m_pOutputType = pmt;
	_RPTFW0(_CRT_WARN, L"CodecBase::OnSetOutputType Output Type Set\n");
	return S_OK;
}

STDMETHODIMP CvgmSampleRenderer::ProcessMessage( 
	MFT_MESSAGE_TYPE eMessage,
	ULONG_PTR ulParam) noexcept
{
	UNREFERENCED_PARAMETER(ulParam);
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = S_OK;

	switch (eMessage)
	{
	case MFT_MESSAGE_COMMAND_FLUSH:
		this->m_outputQueue.fill({ 0, 0 });
		this->m_queueLocation = this->m_outputQueue.begin();
		//ClearQueue(this->m_outputQueue);
	case MFT_MESSAGE_COMMAND_DRAIN:
	case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
	case MFT_MESSAGE_NOTIFY_END_STREAMING:
	case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
	case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
		break;
	default:
		// The pipeline should never send this message unless the MFT
		// has the MF_SA_D3D_AWARE attribute set to TRUE. However, if we
		// do get this message, it's invalid and we don't implement it.
		hr = E_NOTIMPL;
		break;    

	}
	//this->Unlock();
	return hr;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetInputCurrentType(
	DWORD           dwInputStreamID,
	IMFMediaType    **ppType
	) noexcept
{
	if (ppType == nullptr)
	{
		return E_POINTER;
	}

	if (!IsValidInputStream(dwInputStreamID))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = S_OK;

	if (!m_pInputType)
	{
		hr = MF_E_TRANSFORM_TYPE_NOT_SET;
	}

	if (SUCCEEDED(hr))
	{
		this->m_pInputType->QueryInterface(IID_PPV_ARGS(ppType));
	}
	//this->Unlock();
	return hr;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::GetOutputCurrentType(
	DWORD           dwOutputStreamID,
	IMFMediaType    **ppType
	) noexcept
{
	if (ppType == nullptr)
	{
		return E_POINTER;
	}

	if (!IsValidOutputStream(dwOutputStreamID))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = S_OK;

	if (!m_pOutputType)
	{
		hr = MF_E_TRANSFORM_TYPE_NOT_SET;
	}

	if (SUCCEEDED(hr))
	{
		m_pOutputType->QueryInterface(IID_PPV_ARGS(ppType));
	}
	//this->Unlock();
	return hr;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::ProcessOutput( 
	DWORD dwFlags,
	DWORD cOutputBufferCount,
	/* [size_is][out][in] */ MFT_OUTPUT_DATA_BUFFER *pOutputSamples,
	/* [out] */ DWORD *pdwStatus) noexcept
{
	if (dwFlags != 0)
	{
		return E_INVALIDARG;
	}

	if (pOutputSamples == nullptr || pdwStatus == nullptr)
	{
		return E_POINTER;
	}

	// Must be exactly one output buffer.
	if (cOutputBufferCount != 1)
	{
		return E_INVALIDARG;
	}

	// It must contain a sample.
	if (pOutputSamples[0].pSample == nullptr)
	{
		return E_INVALIDARG;
	}
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = S_OK;

	// If we don't have an input sample, we need some input before
	// we can generate any output.
	if (!HasPendingOutput())
	{
		hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
	}

	ATL::CComPtr<IMFMediaBuffer> pOutput;
	// Get the output buffer.

	if (SUCCEEDED(hr))
	{
		hr = pOutputSamples[0].pSample->GetBufferByIndex(0, &pOutput);
	}

	DWORD cbData = 0;
	if (SUCCEEDED(hr))
	{
		hr = pOutput->GetMaxLength(std::addressof(cbData));
	}

	if (SUCCEEDED(hr))
	{		
		if (cbData < AVG_OUTPUT_BYTES_PER_SEC(VGM_SAMPLE_RATE))
		{
			hr = E_INVALIDARG;
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = InternalProcessOutput(pOutputSamples[0].pSample, pOutput);
	}

	if (SUCCEEDED(hr))
	{
		//  Update our state
		//m_bPicture = false;

		//  Is there any more data to output at this point?
		if (S_OK == Process()) 
		{
			//pOutputSamples[0].dwStatus |= MFT_OUTPUT_DATA_BUFFER_INCOMPLETE;
		}
	}    

	//this->Unlock();
	return hr;
}

COM_DECLSPEC_NOTHROW HRESULT CvgmSampleRenderer::InternalProcessOutput(IMFSample *pSample, IMFMediaBuffer *pOutputBuffer) noexcept
{
	if(pSample == nullptr || pOutputBuffer == nullptr)
	{
		return E_POINTER;
	}
	::vgmcodec::helpers::bufferLocker locker(*pOutputBuffer);
	HRESULT hr = locker;

	if (SUCCEEDED(hr))
	{
		std::fill(locker.begin(), locker.end(), 0);
		//SecureZeroMemory(locker.pData, locker.maxLength);
		memcpy_s(locker.pData, locker.maxLength, this->m_outputQueue.data(), this->m_outputQueue.size() * sizeof(outputQueue::value_type));
		this->m_outputQueue.fill({ 0, 0 });
		this->m_queueLocation = this->m_outputQueue.begin();
	}

	if(SUCCEEDED(hr))
	{
		hr = pOutputBuffer->SetCurrentLength(AVG_OUTPUT_BYTES_PER_SEC(VGM_SAMPLE_RATE));
	}

	if (SUCCEEDED(hr))
	{
		hr = pSample->SetSampleDuration(10000000);
	}
	return hr;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::SetOutputBounds( 
	LONGLONG hnsLowerBound,
	LONGLONG hnsUpperBound) noexcept
{
	UNREFERENCED_PARAMETER(hnsLowerBound);
	UNREFERENCED_PARAMETER(hnsUpperBound);
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW STDMETHODIMP CvgmSampleRenderer::ProcessInput( 
	DWORD dwInputStreamID,
	IMFSample *pSample,
	DWORD dwFlags) noexcept
{
	if (pSample == nullptr)
	{
		return E_POINTER;
	}

	if (!IsValidInputStream(dwInputStreamID))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}

	if (dwFlags != 0)
	{
		return E_INVALIDARG; // dwFlags is reserved and must be zero.
	}
	vgmcodec::helpers::lockHelper lock(*this);
	
	HRESULT hr = S_OK;
//	LONGLONG rtTimestamp = 0;
	
	if (!m_pInputType || !m_pOutputType)
	{
		hr = MF_E_NOTACCEPTING;   // Client must set input and output types.
	}
	else if (HasPendingOutput())
	{
		hr = MF_E_NOTACCEPTING;   // We already have an input sample.
	}
	ATL::CComPtr<IMFMediaBuffer> buffer;
	if (SUCCEEDED(hr))
	{   
		// NOTE: This does not cause a copy unless there are multiple buffers
		hr = pSample->ConvertToContiguousBuffer(&buffer);
	}
	
	if (FAILED(hr))
	{
		return hr;
	}
	
	::vgmcodec::helpers::bufferLocker locker(*buffer);
	hr = locker;
	

	if (SUCCEEDED(hr))
	{
		DWORD count = locker.currentLength / sizeof(vgmcodec::sample::VgmSample);
		vgmcodec::sample::VgmSample * samples = reinterpret_cast<vgmcodec::sample::VgmSample*>(locker.pData);
		for(DWORD i = 0; i < count; ++i)
			this->m_inputQueue.push(samples[i]);
	}

	if(SUCCEEDED(hr))
	{
		hr = this->Process();
	}

	//this->Unlock();
	return hr;
}

HRESULT CvgmSampleRenderer::QueueSample(vgmcodec::sample::wsample_t &sample)
{
	if (HasPendingOutput())
		throw std::domain_error("attempting to queue a sample while output is pending!");
	// ChipData << 9 [ChipVol] >> 5 << 8 [MstVol] >> 11  ->  9-5+8-11 = <<1
	
	this->m_queueLocation->left = LimitToShort(((sample.left >> 5) * 256) >> 11);
	this->m_queueLocation->right = LimitToShort(((sample.right >> 5) * 256) >> 11);
	++this->m_queueLocation;
	++this->m_samplesPlayed;
	return S_OK;

}

HRESULT CvgmSampleRenderer::Process(void) noexcept
{
	while(!this->HasPendingOutput())
	{
		size_t samplesPlayed = this->m_samplesPlayed + 1;
		while (this->m_samplesPosition <= samplesPlayed)
		{
			try{
				if (!this->ProcessSample()){
					return S_OK;
				}
			}
			catch (const std::domain_error)
			{
				return MF_PARSE_ERR;
			}
			catch (...)
			{
				return E_FAIL;
			}
		}

		vgmcodec::sample::wsample_t sample = { 0, 0 };
		for (const auto& chip : this->m_vChips){
			chip->Render(sample);
		}
		this->QueueSample(sample);
	}
	
	return S_OK;
}

bool CvgmSampleRenderer::ProcessSample()
{
	if (this->m_inputQueue.empty()){
		return false;
	}
	while (!this->m_inputQueue.empty()){
		vgmcodec::sample::VgmSample sample = this->m_inputQueue.front();
		switch (sample.opCode)
		{
		case 0x4F: // dd    : Game Gear PSG stereo, write dd to port 0x06
		case 0x50: // dd    : PSG (SN76489/SN76496) write value dd
		case 0x51: // aa dd : YM2413, write value dd to register aa
		case 0x52: // aa dd : YM2612 port 0, write value dd to register aa
		case 0x53: // aa dd : YM2612 port 1, write value dd to register aa
		case 0x54: // aa dd : YM2151, write value dd to register aa
		case 0x55: // aa dd : YM2203, write value dd to register aa
		case 0x56: // aa dd : YM2608 port 0, write value dd to register aa
		case 0x57: // aa dd : YM2608 port 1, write value dd to register aa
		case 0x58: // aa dd : YM2610 port 0, write value dd to register aa
		case 0x59: // aa dd : YM2610 port 1, write value dd to register aa
		case 0x5A: // aa dd : YM3812, write value dd to register aa
		case 0x5B: // aa dd : YM3526, write value dd to register aa
		case 0x5C: // aa dd : Y8950, write value dd to register aa
		case 0x5D: // aa dd : YMZ280B, write value dd to register aa
		case 0x5E: // aa dd : YMF262 port 0, write value dd to register aa
		case 0x5F: // aa dd : YMF262 port 1, write value dd to register aa
			for (const auto & chip : this->m_vChips)
				chip->Write(sample);
			break;
		case 0x61: // nn nn : Wait n samples, n can range from 0 to 65535 (approx 1.49
			//         seconds). Longer pauses than this are represented by multiple
			//         wait commands.
			m_samplesPosition += sample.sOps[0];
			break;
		case 0x62:
			m_samplesPosition += 735;
			break;
		case 0x63:
			m_samplesPosition += 882;
			break;
		case 0x70: // 0x7n  : wait n+1 samples, n can range from 0 to 15.
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
		case 0x78:
		case 0x79:
		case 0x7A:
		case 0x7B:
		case 0x7C:
		case 0x7D:
		case 0x7F:
			m_samplesPosition += (sample.opCode & 0x0F) + 1;
			break;
		case 0x66: // EOF
			break;
		default:
			throw std::domain_error("invalid op code");
		}
		this->m_inputQueue.pop();
	}
	return true;
}

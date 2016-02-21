// vgmSource.cpp : Implementation of CvgmSource

#include "stdafx.h"
#include "vgmSource.h"
#include "propVariant.h"
#include "vgmStream.h"
#include "lockHelper.h"


using namespace vgmcodec::io;
using namespace vgmcodec::parser;

STDMETHODIMP QueueEventWithIUnknown(
	IMFMediaEventGenerator *pMEG,
	MediaEventType meType,
	HRESULT hrStatus,
	IUnknown *pUnk);

HRESULT CvgmSource::FinalConstruct() _NOEXCEPT
{
	return MFCreateEventQueue(&m_pEventQueue);
}

// CvgmSource
IFACEMETHODIMP CvgmSource::GetCharacteristics(
	/* [out] */ __RPC__out DWORD *pdwCharacteristics) noexcept
{
	if (pdwCharacteristics == nullptr)
	{
		return E_POINTER;
	}

	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		*pdwCharacteristics = MFMEDIASOURCE_CAN_PAUSE | MFMEDIASOURCE_CAN_SEEK;
	}

	//this->Unlock();

	return hr;
}

IFACEMETHODIMP CvgmSource::CreatePresentationDescriptor(
	/* [annotation][out] */
	__out  IMFPresentationDescriptor **ppPresentationDescriptor) noexcept
{
	if (ppPresentationDescriptor == nullptr)
	{
		return E_POINTER;
	}
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		if (m_pPresentationDescriptor == nullptr)
		{
			hr = CreatePresentationDescriptor();
		}
	}

	// Clone our default presentation descriptor.
	if (SUCCEEDED(hr))
	{
		hr = m_pPresentationDescriptor->Clone(ppPresentationDescriptor);
	}

	return hr;
}

IFACEMETHODIMP CvgmSource::Start(
	/* [in] */ __RPC__in_opt IMFPresentationDescriptor *pPresentationDescriptor,
	/* [unique][in] */ __RPC__in_opt const GUID *pguidTimeFormat,
	/* [unique][in] */ __RPC__in_opt const PROPVARIANT *pvarStartPosition) _NOEXCEPT
{
	// Check parameters. 
	// Start position and presentation descriptor cannot be NULL.
	if (pvarStartPosition == nullptr || pPresentationDescriptor == nullptr)
	{
		return E_INVALIDARG;
	}

	// Check the time format. Must be "reference time" units.
	if ((pguidTimeFormat != nullptr) && (*pguidTimeFormat != GUID_NULL))
	{
		// Unrecognized time format GUID.
		return MF_E_UNSUPPORTED_TIME_FORMAT;
	}

	LONGLONG llStartOffset = 0;
	bool bIsSeek = false;
	bool bIsRestartFromCurrentPosition = false;
	bool bQueuedStartEvent = false;
	propVariant var;
	ATL::CComPtr<IMFMediaEvent> pEvent;
	vgmcodec::helpers::lockHelper lock(*this);

	try{

		// Fail if the source is shut down.
		_com_util::CheckError(CheckShutdown());

		// Check the start position.
		if (pvarStartPosition->vt == VT_I8)
		{
			// Start position is given in pvarStartPosition in 100-ns units.
			llStartOffset = pvarStartPosition->hVal.QuadPart;

			if (m_state != ::VGM_PLAY_STATE::STATE_STOPPED)
			{
				// Source is running or paused, so this is a seek.
				bIsSeek = true;
			}
		}
		else if (pvarStartPosition->vt == VT_EMPTY)
		{
			// Start position is "current position". 
			// For stopped, that means 0. Otherwise, use the current position.
			if (m_state == ::VGM_PLAY_STATE::STATE_STOPPED)
			{
				llStartOffset = 0;
			}
			else
			{
				//llStartOffset = GetCurrentPosition();
				bIsRestartFromCurrentPosition = true;
			}
		}
		else
		{
			// We don't support this time format.
			_com_issue_error(MF_E_UNSUPPORTED_TIME_FORMAT);

		}

		// Validate the caller's presentation descriptor.
		_com_util::CheckError(this->ValidatePresentationDescriptor(pPresentationDescriptor));

		// Sends the MENewStream or MEUpdatedStream event.
		_com_util::CheckError(QueueNewStreamEvent(pPresentationDescriptor));

		// Notify the stream of the new start time.
		_com_util::CheckError(m_pStream->SetPosition(llStartOffset));

		// Send Started or Seeked events. 

		var = llStartOffset;

		// Send the source event.
		if (bIsSeek)
		{
			_com_util::CheckError(QueueEvent(MESourceSeeked, GUID_NULL, S_OK, &var));
		}
		else
		{
			// For starting, if we are RESTARTING from the current position and our 
			// previous state was running/paused, then we need to add the 
			// MF_EVENT_SOURCE_ACTUAL_START attribute to the event. This requires
			// creating the event object first.

			// Create the event.
			_com_util::CheckError(MFCreateMediaEvent(MESourceStarted, GUID_NULL, S_OK, &var, &pEvent));

			// For restarts, set the actual start time as an attribute.
			if (bIsRestartFromCurrentPosition)
			{
				_com_util::CheckError(pEvent->SetUINT64(MF_EVENT_SOURCE_ACTUAL_START, llStartOffset));
			}

			// Now  queue the event.
			_com_util::CheckError(m_pEventQueue->QueueEvent(pEvent));
		}

		bQueuedStartEvent = true;

		// Send the stream event.
		if (m_pStream)
		{
			if (bIsSeek)
			{
				_com_util::CheckError(m_pStream->QueueEvent(MEStreamSeeked, GUID_NULL, S_OK, &var));
			}
			else
			{
				_com_util::CheckError(m_pStream->QueueEvent(MEStreamStarted, GUID_NULL, S_OK, &var));
			}
		}

		if (bIsSeek)
		{
			// For seek requests, flush any queued samples.
			_com_util::CheckError(m_pStream->Flush());
		}
		else
		{
			// Otherwise, deliver any queued samples.
			_com_util::CheckError(m_pStream->DeliverQueuedSamples());
		}

		m_state = ::VGM_PLAY_STATE::STATE_STARTED;
	}
	catch (_com_error const& ex)
	{
		// If a failure occurred and we have not sent the 
		// MESourceStarted/MESourceSeeked event yet, then it is 
		// OK just to return an error code from Start().

		// If a failure occurred and we have already sent the 
		// event (with a success code), then we need to raise an
		// MEError event.
		if (bQueuedStartEvent)
		{
			return QueueEvent(MEError, GUID_NULL, ex.Error(), &var);
		}
	}
	catch (...)
	{
		if (bQueuedStartEvent){
			return QueueEvent(MEError, GUID_NULL, E_FAIL, &var);
		}
	}

	return S_OK;
}

IFACEMETHODIMP CvgmSource::Stop(void) _NOEXCEPT
{
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		// Update our state. 
		m_state = ::VGM_PLAY_STATE::STATE_STOPPED;

		// Flush all queued samples.
		hr = m_pStream->Flush();
	}

	////
	//// Queue events.
	////

	if (SUCCEEDED(hr))
	{
		if (m_pStream)
		{
			hr = m_pStream->QueueEvent(MEStreamStopped, GUID_NULL, S_OK, nullptr);
		}
	}
	if (SUCCEEDED(hr))
	{
		hr = QueueEvent(MESourceStopped, GUID_NULL, S_OK, nullptr);
	}

	return hr;
}

IFACEMETHODIMP CvgmSource::Pause(void) _NOEXCEPT
{
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = CheckShutdown();

	// Pause is only allowed from started state.
	if (SUCCEEDED(hr))
	{
		if (m_state != ::VGM_PLAY_STATE::STATE_STARTED)
		{
			hr = MF_E_INVALID_STATE_TRANSITION;
		}
	}

	// Send the appropriate events.
	if (SUCCEEDED(hr))
	{
		if (m_pStream)
		{
			hr = m_pStream->QueueEvent(MEStreamPaused, GUID_NULL, S_OK, nullptr);
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = QueueEvent(MESourcePaused, GUID_NULL, S_OK, nullptr);
	}

	// Update our state. 
	if (SUCCEEDED(hr))
	{
		m_state = ::VGM_PLAY_STATE::STATE_PAUSED;
	}

	return hr;
}

IFACEMETHODIMP CvgmSource::Shutdown(void) _NOEXCEPT
{
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		//// Shut down the stream object.
		if (m_pStream)
		{
			m_pStream->Shutdown();
		}

		// Shut down the event queue.
		if (m_pEventQueue)
		{
			m_pEventQueue->Shutdown();
		}

		// Release objects. 
		this->m_pEventQueue.Release();
		this->m_pPresentationDescriptor.Release();
		this->m_pStream.Release();
		this->m_pParser.reset();

		// Set our shutdown flag.
		m_IsShutdown = true;
	}

	return hr;
}


IFACEMETHODIMP CvgmSource::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState) _NOEXCEPT
{
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = CheckShutdown();

	if (SUCCEEDED(hr))
		hr = m_pEventQueue->BeginGetEvent(pCallback, punkState);

	return hr;
}

IFACEMETHODIMP CvgmSource::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent) _NOEXCEPT
{
	vgmcodec::helpers::lockHelper lock(*this);

	HRESULT hr = CheckShutdown();

	if (SUCCEEDED(hr))
		hr = m_pEventQueue->EndGetEvent(pResult, ppEvent);

	return hr;
}

IFACEMETHODIMP CvgmSource::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent) _NOEXCEPT
{
	ATL::CComPtr<IMFMediaEventQueue> pQueue;
	
	vgmcodec::helpers::lockHelper lock(*this);
	HRESULT hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		pQueue = m_pEventQueue;
	}

	if (SUCCEEDED(hr))
		hr = pQueue->GetEvent(dwFlags, ppEvent);

	return hr;
}

IFACEMETHODIMP CvgmSource::QueueEvent(
	MediaEventType met,
	REFGUID guidExtendedType,
	HRESULT hrStatus,
	const PROPVARIANT* pvValue) _NOEXCEPT
{
	vgmcodec::helpers::lockHelper lock(*this);
	
	HRESULT hr = CheckShutdown();

	if (SUCCEEDED(hr))
		hr = m_pEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);

	return hr;
}

HRESULT CvgmSource::CreatePresentationDescriptor(void) 
{
	ATL::CComPtr<IMFMediaType> pMediaType;
	ATL::CComPtr<IMFStreamDescriptor> pStreamDescriptor;
	ATL::CComPtr<IMFMediaTypeHandler> pHandler;

	try{
		// Create an empty media type.
		_com_util::CheckError(MFCreateMediaType(&pMediaType));

		_com_util::CheckError(pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));

		_com_util::CheckError(pMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_VGM));


		std::vector<UINT8> headerBuff(sizeof(vgmcodec::format::vgmHeader));
		vgmcodec::format::vgmHeader header = this->m_pParser->getHeader();
		memcpy_s(
			reinterpret_cast<void*>(std::addressof(headerBuff[0])),
			sizeof(vgmcodec::format::vgmHeader),
			reinterpret_cast<void*>(std::addressof(header)),
			sizeof(vgmcodec::format::vgmHeader));
		_com_util::CheckError(
			pMediaType->SetBlob(
			MF_MT_VGM_HEADER,
			headerBuff.data(),
			static_cast<UINT32>(headerBuff.size())));
		


		// Create the stream descriptor.
		_com_util::CheckError(
			MFCreateStreamDescriptor(
			0,          // stream identifier
			1,          // Number of media types.
			std::addressof(pMediaType.p), // Array of media types
			&pStreamDescriptor
			));

		// Set the default media type on the media type handler.

		_com_util::CheckError(pStreamDescriptor->GetMediaTypeHandler(&pHandler));

		_com_util::CheckError(pHandler->SetCurrentMediaType(pMediaType));

		// Create the presentation descriptor.
		_com_util::CheckError(MFCreatePresentationDescriptor(
			1,                      // Number of stream descriptors
			std::addressof(pStreamDescriptor.p),     // Array of stream descriptors
			&m_pPresentationDescriptor
			));

		// Select the first stream
		_com_util::CheckError(m_pPresentationDescriptor->SelectStream(0));
		
		MFTIME duration = (static_cast<MFTIME>(header.sampleCount) * 10000000) / VGM_SAMPLE_RATE;
		_com_util::CheckError(m_pPresentationDescriptor->SetUINT64(MF_PD_DURATION, static_cast<UINT64>(duration)));

		_com_util::CheckError(m_pPresentationDescriptor->SetString(MF_PD_MIME_TYPE, L"audio/x-vgm"));
	}
	catch (_com_error const & ex){
		return ex.Error();
	}
	catch (...){
		return E_FAIL;
	}
	return S_OK;
}


IFACEMETHODIMP CvgmSource::GetPlayState(REF_VGM_PLAY_STATE pState) _NOEXCEPT
{
	vgmcodec::helpers::lockHelper lock(*this);
	pState = this->m_state;
	return S_OK;
}

HRESULT CvgmSource::ValidatePresentationDescriptor(gsl::not_null<IMFPresentationDescriptor*> pPresentationDescriptor)
{
	ATL::CComPtr<IMFStreamDescriptor> pStreamDescriptor;
	ATL::CComPtr<IMFMediaTypeHandler> pHandler;
	ATL::CComPtr<IMFMediaType> pMediaType;

	DWORD   cStreamDescriptors = 0;
	BOOL    fSelected = FALSE;
	GUID majorType = { 0 };
	GUID subtypeType = { 0 };

	// Make sure there is only one stream.
	HRESULT hr = pPresentationDescriptor->GetStreamDescriptorCount(&cStreamDescriptors);

	if (SUCCEEDED(hr))
	{
		if (cStreamDescriptors != 1)
		{
			hr = MF_E_UNSUPPORTED_REPRESENTATION;
		}
	}

	// Get the stream descriptor.
	if (SUCCEEDED(hr))
	{
		hr = pPresentationDescriptor->GetStreamDescriptorByIndex(0, &fSelected, &pStreamDescriptor);
	}

	// Make sure it's selected. (This media source has only one stream, so it
	// is not useful to deselect the only stream.)
	if (SUCCEEDED(hr))
	{
		if (!fSelected)
		{
			hr = MF_E_UNSUPPORTED_REPRESENTATION;
		}
	}

	// Get the media type handler, so that we can get the media type.
	if (SUCCEEDED(hr))
	{
		hr = pStreamDescriptor->GetMediaTypeHandler(&pHandler);
	}

	if (SUCCEEDED(hr))
	{
		hr = pHandler->GetCurrentMediaType(&pMediaType);
	}

	if (SUCCEEDED(hr))
	{
		hr = pMediaType->GetMajorType(&majorType);
	}

	if (SUCCEEDED(hr))
	{
		assert(!IsEqualGUID(majorType, GUID_NULL));

		if (!IsEqualGUID(majorType, MFMediaType_Audio))
		{
			hr = MF_E_INVALIDMEDIATYPE;
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = pMediaType->GetGUID(MF_MT_SUBTYPE, &subtypeType);
	}

	if (SUCCEEDED(hr))
	{
		assert(!IsEqualGUID(subtypeType, GUID_NULL));

		// TODO: extend this later
		if (!IsEqualGUID(subtypeType, MFAudioFormat_VGM))
		{
			hr = MF_E_INVALIDMEDIATYPE;
		}
	}

	return hr;
}

HRESULT CvgmSource::QueueNewStreamEvent(gsl::not_null<IMFPresentationDescriptor*> pPD)
{
	ATL::CComPtr<IMFStreamDescriptor> pSD;

	BOOL fSelected = FALSE;

	HRESULT hr = pPD->GetStreamDescriptorByIndex(0, &fSelected, &pSD);

	if (SUCCEEDED(hr))
	{
		// The stream must be selected, because we don't allow the app
		// to de-select the stream. See ValidatePresentationDescriptor.
		assert(fSelected);

		if (m_pStream)
		{
			// The stream already exists, and is still selected.
			// Send the MEUpdatedStream event.
			hr = QueueEventWithIUnknown(this, MEUpdatedStream, S_OK, m_pStream);
		}
		else
		{
			// The stream does not exist, and is now selected.
			// Create a new stream.

			this->createStream(pSD);
			if (SUCCEEDED(hr))
			{
				// CreateWavStream creates the stream, so m_pStream is no longer NULL.
				assert(m_pStream != nullptr);

				// Send the MENewStream event.
				hr = QueueEventWithIUnknown(this, MENewStream, S_OK, m_pStream);
			}
		}
	}

	return hr;
}

HRESULT CvgmSource::Open(IMFByteStream * pOutputStream) noexcept {
	if (pOutputStream == nullptr)
	{
		return E_POINTER;
	}
	vgmcodec::helpers::lockHelper lock(*this);

	if (this->m_pParser){
		return MF_E_INVALIDREQUEST;
	}
	try{
		this->m_pParser = std::make_shared<vgmParser>(pOutputStream);
	}
	catch (_com_error const & ex){
		return ex.Error();
	}
	catch (...){
		return E_FAIL;
	}
	return S_OK;
}

HRESULT CvgmSource::createStream(ATL::CComPtr<IMFStreamDescriptor> const& pPD)
{
	HRESULT hr = this->m_pStream.CoCreateInstance(CLSID_vgmStream);
	if (SUCCEEDED(hr))
	{
		hr = (dynamic_cast<CvgmStream*>(this->m_pStream.p))->Init(this, pPD, this->m_pParser);
	}
	return hr;
}
// vgmStream.cpp : Implementation of CvgmStream

#include "stdafx.h"
#include "vgmStream.h"
#include "vgmSource.h"
#include "propVariant.h"
#include "lockHelper.h"
#include "bufferLocker.h"
using namespace vgmcodec::io;
using namespace vgmcodec::parser;

STDMETHODIMP QueueEventWithIUnknown(
	IMFMediaEventGenerator *pMEG,
	MediaEventType meType,
	HRESULT hrStatus,
	IUnknown *pUnk)
{

	// Create the PROPVARIANT to hold the IUnknown value.
	propVariant var(pUnk);

	// Queue the event.
	HRESULT hr = pMEG->QueueEvent(meType, GUID_NULL, hrStatus, std::addressof(var));

	return hr;
}

CvgmStream::CvgmStream(void) 
	:m_IsShutdown(false),
	m_rtCurrentPosition(0),
	m_discontinuity(false),
	m_EOS(false)
{

}

HRESULT CvgmStream::FinalConstruct()
{
	return MFCreateEventQueue(&m_pEventQueue);
}

// CvgmStream
IFACEMETHODIMP CvgmStream::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState)
{
	HRESULT hr = S_OK;
	vgmcodec::helpers::lockHelper lock(*this);

	hr = CheckShutdown();
	if (SUCCEEDED(hr))
	{   
		hr = m_pEventQueue->BeginGetEvent(pCallback, punkState);
	}

	return hr;
}

IFACEMETHODIMP CvgmStream::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
	HRESULT hr = S_OK;

	vgmcodec::helpers::lockHelper lock(*this);

	hr = CheckShutdown();
	if (SUCCEEDED(hr))
	{   
		hr = m_pEventQueue->EndGetEvent(pResult, ppEvent);
	}

	return hr;
}

IFACEMETHODIMP CvgmStream::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
	HRESULT hr = S_OK;

	ATL::CComPtr<IMFMediaEventQueue> pQueue;
	{
		vgmcodec::helpers::lockHelper lock(*this);

		hr = CheckShutdown();
		// Store the pointer in a local variable, so that another thread
		// does not release it after we leave the critical section.
		if (SUCCEEDED(hr))
		{
			pQueue = m_pEventQueue;
		}
	} //this->Unlock();
	
	if (SUCCEEDED(hr))   
		hr = pQueue->GetEvent(dwFlags, ppEvent);

	return hr;
}

IFACEMETHODIMP CvgmStream::QueueEvent(
	MediaEventType met,
	REFGUID guidExtendedType,
	HRESULT hrStatus,
	const PROPVARIANT* pvValue)
{
	HRESULT hr = S_OK;

	vgmcodec::helpers::lockHelper lock(*this);

	hr = CheckShutdown();
	if (SUCCEEDED(hr))   
		hr = m_pEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);

	return hr;
}

IFACEMETHODIMP CvgmStream::GetMediaSource(IMFMediaSource** ppMediaSource)
{
	if (ppMediaSource == nullptr)
		return E_POINTER;

	vgmcodec::helpers::lockHelper lock(*this);
	HRESULT hr = S_OK;

	// If called after shutdown, them m_pSource is NULL.
	// Otherwise, m_pSource should not be NULL.

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{   
		if (m_pSource == nullptr)
			hr = E_UNEXPECTED;
	}

	if (SUCCEEDED(hr))
	{   
		hr = m_pSource.QueryInterface(ppMediaSource);
	}

	return hr;
}

IFACEMETHODIMP CvgmStream::GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor)
{
	if (ppStreamDescriptor == nullptr)
		return E_POINTER;

	if (!this->m_pStreamDescriptor)
		return E_UNEXPECTED;

	vgmcodec::helpers::lockHelper lock(*this);
	HRESULT hr = S_OK;

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{   
		hr = m_pStreamDescriptor.QueryInterface<IMFStreamDescriptor>(ppStreamDescriptor);
	}

	return hr;
}

IFACEMETHODIMP CvgmStream::RequestSample(IUnknown* pToken)
{
	if (m_pSource == nullptr)
		return E_UNEXPECTED;

	HRESULT hr = S_OK;

	ATL::CComPtr<IMFMediaSource> pSource;
	ATL::CComPtr<IMFSample> pSample;  // Sample to deliver.
	{	// lock scope
		vgmcodec::helpers::lockHelper lock(*this);

		// Check if we are shut down.
		hr = CheckShutdown();

		// Check if we already reached the end of the stream.
		if (SUCCEEDED(hr))
		{
			if (m_EOS)
				hr = MF_E_END_OF_STREAM;
		}

		// Check the source is stopped.
		// GetState does not hold the source's critical section. Safe to call.
		if (SUCCEEDED(hr))
		{
			VGM_PLAY_STATE state = VGM_PLAY_STATE::STATE_STOPPED;
			m_pSource->GetPlayState(state);
			if (state == VGM_PLAY_STATE::STATE_STOPPED)
				hr = MF_E_INVALIDREQUEST;
		}

		if (SUCCEEDED(hr))
		{
			try{
				// Create a new audio sample.
				pSample = CreateAudioSample();
			}
			catch (_com_error const & ex)
			{
				hr = ex.Error();
			}
			catch (...){
				return E_FAIL;
			}
		}

		if (SUCCEEDED(hr))
		{
			// If the caller provided a token, attach it to the sample as
			// an attribute. 

			// NOTE: If we processed sample requests asynchronously, we would
			// need to call AddRef on the token and put the token onto a FIFO
			// queue. See documentation for IMFMediaStream::RequestSample.
			if (pToken)
				hr = pSample->SetUnknown(MFSampleExtension_Token, pToken);
		}

		// If paused, queue the sample for later delivery. Otherwise, deliver the sample now.
		if (SUCCEEDED(hr))
		{
			VGM_PLAY_STATE state = ::VGM_PLAY_STATE::STATE_STOPPED;
			m_pSource->GetPlayState(state);
			if (state == ::VGM_PLAY_STATE::STATE_PAUSED)
				m_sampleQueue.push(pSample);
			else
				hr = DeliverSample(pSample);
		}

		// Cache a pointer to the source, prior to leaving the critical section.
		if (SUCCEEDED(hr))
		{
			//this->m_pSource.QueryInterface(pSource);
			pSource = m_pSource;
			//(*pSource).AddRef();
		}
	} // lock scope

	//this->Unlock();


	// We only have one stream, so the end of the stream is also the end of the
	// presentation. Therefore, when we reach the end of the stream, we need to 
	// queue the end-of-presentation event from the source. Logically we would do 
	// this inside the CheckEndOfStream method. However, we cannot hold the
	// source's critical section while holding the stream's critical section, at
	// risk of deadlock. 

	if (SUCCEEDED(hr))
	{   
		if (m_EOS)
			hr = pSource->QueueEvent(MEEndOfPresentation, GUID_NULL, S_OK, nullptr);
	}

	return hr;
}

IFACEMETHODIMP CvgmStream::Shutdown(void)
{
	vgmcodec::helpers::lockHelper lock(*this);
	// Flush queued samples.
	Flush();

	// Shut down the event queue.
	if (m_pEventQueue)
	{
		m_pEventQueue->Shutdown();
	}

	this->m_pEventQueue.Release();
	this->m_pStreamDescriptor.Release();
	this->m_pSource.Release();

	m_IsShutdown = true;

	return S_OK;
}

ATL::CComPtr<IMFSample> CvgmStream::CreateAudioSample()
{
	ATL::CComPtr<IMFMediaBuffer> pBuffer;
	ATL::CComPtr<IMFSample> pSample;

	LONGLONG    duration = 0;

	// Start with one second of data, rounded up to the nearest block.
	const DWORD cbBuffer = sizeof(vgmcodec::sample::VgmSample);

	// Create the buffer.
	_com_util::CheckError(MFCreateMemoryBuffer(cbBuffer, &pBuffer));

	// Get a pointer to the buffer memory.
	{
		::vgmcodec::helpers::bufferLocker locker(*pBuffer);
		_com_util::CheckError(locker);

		vgmcodec::sample::VgmSample sample;
		if (!this->m_pParser->getNextSample(sample)){
			this->m_EOS = true;
		}
		// Fill the buffer
		memcpy_s(locker.pData, cbBuffer, std::addressof(sample), cbBuffer);
	}


	// Set the size of the valid data in the buffer.
	_com_util::CheckError(pBuffer->SetCurrentLength(cbBuffer));

	// Create a new sample and add the buffer to it.
	_com_util::CheckError(MFCreateSample(&pSample));
  
	_com_util::CheckError(pSample->AddBuffer(pBuffer));
 
	_com_util::CheckError(pSample->SetSampleTime(m_rtCurrentPosition));
 
		//duration = AudioDurationFromBufferSize(m_pRiff->Format(), cbBuffer);
	_com_util::CheckError(pSample->SetSampleDuration(duration));

	// Set the discontinuity flag.
	if (m_discontinuity)
	{
		_com_util::CheckError(pSample->SetUINT32(MFSampleExtension_Discontinuity, TRUE));
	}

		// Update our current position.
	m_rtCurrentPosition += duration;

	return pSample;
}

HRESULT CvgmStream::DeliverSample(IMFSample *pSample)
{
	HRESULT hr = S_OK;

	// Send the MEMediaSample event with the new sample.
	hr = QueueEventWithIUnknown(this, MEMediaSample, hr, pSample); 

	// See if we reached the end of the stream.
	if (SUCCEEDED(hr))
	{   
		hr = CheckEndOfStream();    // This method sends MEEndOfStream if needed.
	}

	return hr;
}

HRESULT CvgmStream::DeliverQueuedSamples(void)
{
	HRESULT hr = S_OK;
	ATL::CComPtr<IMFSample> pSample;

	vgmcodec::helpers::lockHelper lock(*this);

	// If we already reached the end of the stream, send the MEEndStream 
	// event again.
	if (m_EOS)
	{
		hr = QueueEvent(MEEndOfStream, GUID_NULL, S_OK, nullptr);
	}

	if (SUCCEEDED(hr))
	{   
		// Deliver any queued samples. 
		while (!m_sampleQueue.empty())
		{
			pSample = m_sampleQueue.front();
			m_sampleQueue.pop();

			hr = DeliverSample(pSample);
		}
	}

	//this->Unlock();

	// If we reached the end of the stream, send the end-of-presentation event from
	// the media source.
	if (SUCCEEDED(hr))
	{   
		if (m_EOS)
		{
			hr = m_pSource->QueueEvent(MEEndOfPresentation, GUID_NULL, S_OK, NULL);
		}
	}

	return hr;
}

HRESULT CvgmStream::Flush(void)
{
	vgmcodec::helpers::lockHelper lock(*this);
	
	ClearQueue(m_sampleQueue);
	//this->m_sampleQueue.Clear();

	return S_OK;
}

HRESULT CvgmStream::SetPosition(LONGLONG rtNewPosition)
{
	vgmcodec::helpers::lockHelper lock(*this);
	// Check if the requested position is beyond the end of the stream.
	// LONGLONG duration = AudioDurationFromBufferSize(m_pRiff->Format(), m_pRiff->Chunk().DataSize());

	//if (rtNewPosition > duration)
	//{
	//    LeaveCriticalSection(&m_critSec);

	//    return MF_E_INVALIDREQUEST; // Start position is past the end of the presentation.
	//}

	HRESULT hr = S_OK;

	if (m_rtCurrentPosition != rtNewPosition)
	{
		//LONGLONG offset = BufferSizeFromAudioDuration(m_pRiff->Format(), rtNewPosition);

		// The chunk size is a DWORD. So if our calculations are correct, there is no
		// way that the maximum valid seek position can be larger than a DWORD. 
		// assert(offset <= MAXDWORD);

		//hr = m_pRiff->MoveToChunkOffset((DWORD)offset);

		if (SUCCEEDED(hr))
		{   
			m_rtCurrentPosition = rtNewPosition;
			m_discontinuity = true;
			m_EOS = true;
		}
	}

	return hr;
}

HRESULT CvgmStream::CheckEndOfStream(void)
{
	HRESULT hr = S_OK;

	//if (m_pRiff->BytesRemainingInChunk() < m_pRiff->Format()->nBlockAlign)
	//{
	//    // The remaining data is smaller than the audio block size. (In theory there shouldn't be
	//    // partial bits of data at the end, so we should reach an even zero bytes, but the file
	//    // might not be authored correctly.)
	//    m_EOS = TRUE;

	//    // Send the end-of-stream event,
	//    hr = QueueEvent(MEEndOfStream, GUID_NULL, S_OK, NULL);
	//}
	return hr; 
}

HRESULT CvgmStream::Init(ATL::CComPtr<IVGMSource> const & source, ATL::CComPtr<IMFStreamDescriptor> const & pPD, std::shared_ptr<vgmParser> const& parser)
{
	this->m_pParser = parser;
	this->m_pStreamDescriptor = pPD;
	this->m_pSource = source;
	return S_OK;
}
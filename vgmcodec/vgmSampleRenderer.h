// vgmSampleRenderer.h : Declaration of the CvgmSampleRenderer

#pragma once
#include "resource.h"       // main symbols

#include "stdafx.h"

#include "vgmcodec_i.h"
#include "VgmSample.h"
#include "IChip.h"

namespace vgmcodec{
	namespace transform{
		// CvgmSampleRenderer

		class ATL_NO_VTABLE CvgmSampleRenderer :
			public IMFTransform,
			public ::ATL::CComObjectRootEx<::ATL::CComMultiThreadModel>,
			public ::ATL::CComCoClass<CvgmSampleRenderer, &CLSID_vgmSampleRenderer>
		{
		public:
			CvgmSampleRenderer(void) _NOEXCEPT;

			DECLARE_REGISTRY_RESOURCEID(IDR_VGMSAMPLERENDERER)

			DECLARE_NOT_AGGREGATABLE(CvgmSampleRenderer)

			BEGIN_COM_MAP(CvgmSampleRenderer)
				COM_INTERFACE_ENTRY(IMFTransform)
			END_COM_MAP()



			DECLARE_PROTECT_FINAL_CONSTRUCT()

			HRESULT FinalConstruct(void) _NOEXCEPT
			{
				return S_OK;
			}

			void FinalRelease(void) _NOEXCEPT
			{
			}

		public:
			// IMFTransform
			IFACEMETHOD(GetInputStreamInfo)(
				DWORD dwInputStreamID,
				/* [out] */ __RPC__out MFT_INPUT_STREAM_INFO *pStreamInfo) _NOEXCEPT;
			IFACEMETHOD(GetOutputStreamInfo)( 
				DWORD dwOutputStreamID,
				/* [out] */ __RPC__out MFT_OUTPUT_STREAM_INFO *pStreamInfo) _NOEXCEPT;
			IFACEMETHOD(GetStreamLimits)( 
				/* [out] */ DWORD *pdwInputMinimum,
				/* [out] */ DWORD *pdwInputMaximum,
				/* [out] */ DWORD *pdwOutputMinimum,
				/* [out] */ DWORD *pdwOutputMaximum);
			IFACEMETHOD(GetStreamCount)( 
				/* [out] */ __RPC__out DWORD *pcInputStreams,
				/* [out] */ __RPC__out DWORD *pcOutputStreams) _NOEXCEPT;
			IFACEMETHOD(GetStreamIDs)( 
				DWORD dwInputIDArraySize,
				/* [size_is][out] */ __RPC__out_ecount_full(dwInputIDArraySize) DWORD *pdwInputIDs,
				DWORD dwOutputIDArraySize,
				/* [size_is][out] */ __RPC__out_ecount_full(dwOutputIDArraySize) DWORD *pdwOutputIDs) _NOEXCEPT;
			IFACEMETHOD(GetInputCurrentType)(
				DWORD           dwInputStreamID,
				IMFMediaType    **ppType
				) _NOEXCEPT;
			IFACEMETHODIMP GetOutputCurrentType(
				DWORD           dwOutputStreamID,
				IMFMediaType    **ppType
				) _NOEXCEPT;
			IFACEMETHOD(GetOutputAvailableType)( 
				DWORD dwOutputStreamID,
				DWORD dwTypeIndex,
				/* [out] */ __RPC__deref_out_opt IMFMediaType **ppType) _NOEXCEPT;

			IFACEMETHOD(SetInputType)( 
				DWORD dwInputStreamID,
				/* [in] */ __RPC__in_opt IMFMediaType *pType,
				DWORD dwFlags) _NOEXCEPT;

			IFACEMETHOD(GetAttributes)( 
				/* [out] */ __RPC__deref_out_opt IMFAttributes **pAttributes) _NOEXCEPT;
			IFACEMETHOD(GetInputStreamAttributes)( 
				DWORD dwInputStreamID,
				/* [out] */ __RPC__deref_out_opt IMFAttributes **pAttributes) _NOEXCEPT;
			IFACEMETHOD(GetOutputStreamAttributes)( 
				DWORD dwOutputStreamID,
				/* [out] */ __RPC__deref_out_opt IMFAttributes **pAttributes) _NOEXCEPT;
			IFACEMETHOD(DeleteInputStream)( 
				DWORD dwStreamID) _NOEXCEPT;
			IFACEMETHOD(AddInputStreams)( 
				DWORD cStreams,
				/* [in] */ __RPC__in DWORD *adwStreamIDs) _NOEXCEPT;
			IFACEMETHOD(GetInputAvailableType)( 
				DWORD dwInputStreamID,
				DWORD dwTypeIndex,
				/* [out] */ IMFMediaType **ppType) _NOEXCEPT;
			IFACEMETHOD(SetOutputType)( 
				DWORD dwOutputStreamID,
				/* [in] */ IMFMediaType *pType,
				DWORD dwFlags) _NOEXCEPT;
			IFACEMETHOD(ProcessMessage)( 
				MFT_MESSAGE_TYPE eMessage,
				ULONG_PTR ulParam) ;

			IFACEMETHOD(GetInputStatus)( 
				DWORD dwInputStreamID,
				/* [out] */ __RPC__out DWORD *pdwFlags) ;

			IFACEMETHOD(GetOutputStatus)( 
				/* [out] */ __RPC__out DWORD *pdwFlags) ;

			IFACEMETHOD(SetOutputBounds)( 
				LONGLONG hnsLowerBound,
				LONGLONG hnsUpperBound) ;

			IFACEMETHOD(ProcessEvent)( 
				DWORD dwInputStreamID,
				/* [in] */ __RPC__in_opt IMFMediaEvent *pEvent) ;
			/* [local] */ IFACEMETHOD(ProcessOutput)( 
				DWORD dwFlags,
				DWORD cOutputBufferCount,
				/* [size_is][out][in] */ MFT_OUTPUT_DATA_BUFFER *pOutputSamples,
				/* [out] */ DWORD *pdwStatus) ;
			/* [local] */ IFACEMETHOD(ProcessInput)( 
				DWORD dwInputStreamID,
				IMFSample *pSample,
				DWORD dwFlags) ;

		private:
			using outputQueue = std::array<vgmcodec::sample::sample_t, VGM_SAMPLE_RATE>;
			::ATL::CComPtr<IMFMediaType>   m_pInputType;              // Input media type.
			::ATL::CComPtr<IMFMediaType>   m_pOutputType;             // Output media type.
			::std::queue<vgmcodec::sample::VgmSample>	m_inputQueue;
			::std::vector<std::shared_ptr<vgmcodec::transform::chips::IChip>> m_vChips;
			outputQueue m_outputQueue;
			outputQueue::iterator m_queueLocation;
			//vgmcodec::sample::sample_t				m_outputQueue[VGM_SAMPLE_RATE];

			size_t					m_samplesPosition;
			size_t					m_samplesPlayed;

			HRESULT OnCheckInputType(IMFMediaType *pmt) noexcept;
			HRESULT Process(void) noexcept;
			HRESULT InternalProcessOutput(IMFSample *pSample, IMFMediaBuffer *pOutputBuffer) noexcept;
			bool ProcessSample();
			HRESULT QueueSample(::vgmcodec::sample::wsample_t &sample);

			// HasPendingOutput: Returns TRUE if the MFT is holding an input sample.
			bool HasPendingOutput() const _NOEXCEPT
			{ 
				return this->m_queueLocation == this->m_outputQueue.cend(); 
			}
			// IsValidInputStream: Returns TRUE if dwInputStreamID is a valid input stream identifier.
			bool IsValidInputStream(DWORD dwInputStreamID) const _NOEXCEPT
			{
				return dwInputStreamID == 0;
			}
			// IsValidOutputStream: Returns TRUE if dwOutputStreamID is a valid output stream identifier.
			bool IsValidOutputStream(DWORD dwOutputStreamID) const _NOEXCEPT
			{
				return dwOutputStreamID == 0;
			}

			HRESULT OnSetInputType(IMFMediaType * pmt) _NOEXCEPT;
			HRESULT OnCheckOutputType(IMFMediaType *pmt) _NOEXCEPT;
			HRESULT OnSetOutputType(IMFMediaType *pmt) _NOEXCEPT;
		};

		OBJECT_ENTRY_AUTO(__uuidof(vgmSampleRenderer), CvgmSampleRenderer)
	}
}
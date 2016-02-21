// vgmStream.h : Declaration of the CvgmStream

#pragma once
#include "resource.h"       // main symbols



#include "vgmcodec_i.h"
#include <queue>
#include "vgmSource.h"


namespace vgmcodec{
	namespace io{
		// CvgmStream

		class ATL_NO_VTABLE CvgmStream :
			public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
			public ATL::CComCoClass<CvgmStream, &CLSID_vgmStream>,
			public IVGMStream
		{
		public:
			CvgmStream(void);

			DECLARE_REGISTRY_RESOURCEID(IDR_VGMSTREAM)

			DECLARE_NOT_AGGREGATABLE(CvgmStream)

			BEGIN_COM_MAP(CvgmStream)
				COM_INTERFACE_ENTRY(IMFMediaStream)
				COM_INTERFACE_ENTRY(IVGMStream)
				COM_INTERFACE_ENTRY(IMFMediaEventGenerator)
			END_COM_MAP()



			DECLARE_PROTECT_FINAL_CONSTRUCT()

			HRESULT FinalConstruct(void);

			void FinalRelease()
			{
			}

		public:
			// IMFMediaEventGenerator
			IFACEMETHOD(BeginGetEvent)(IMFAsyncCallback* pCallback,IUnknown* punkState);
			IFACEMETHOD(EndGetEvent)(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
			IFACEMETHOD(GetEvent)(DWORD dwFlags, IMFMediaEvent** ppEvent);
			IFACEMETHOD(QueueEvent)(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue);

			// IMFMediaStream
			IFACEMETHOD(GetMediaSource)(IMFMediaSource** ppMediaSource);
			IFACEMETHOD(GetStreamDescriptor)(IMFStreamDescriptor** ppStreamDescriptor);
			IFACEMETHOD(RequestSample)(IUnknown* pToken);
			IFACEMETHODIMP CheckShutdown(void)
			{
				if (m_IsShutdown)
				{
					return MF_E_SHUTDOWN;
				}
				else
				{
					return S_OK;
				}
			};

			IFACEMETHOD(Shutdown)(void);
			IFACEMETHOD(SetPosition)(LONGLONG rtNewPosition);
			IFACEMETHOD(DeliverQueuedSamples)(void);
			HRESULT Init(ATL::CComPtr<IVGMSource> const & source, ATL::CComPtr<IMFStreamDescriptor> const &, std::shared_ptr<vgmcodec::parser::vgmParser> const& parser);
		private:
			
			ATL::CComPtr<IMFSample>     CreateAudioSample();
			HRESULT     DeliverSample(IMFSample *pSample);
			
			HRESULT     Flush(void);

			LONGLONG    GetCurrentPosition(void) const { return m_rtCurrentPosition; }
			HRESULT     CheckEndOfStream(void);


			bool                        m_IsShutdown;           // Flag to indicate if source's Shutdown() method was called.
			LONGLONG                    m_rtCurrentPosition;    // Current position in the stream, in 100-ns units 
			bool                        m_discontinuity;        // Is the next sample a discontinuity?
			bool                        m_EOS;                  // Did we reach the end of the stream?

			std::shared_ptr<vgmcodec::parser::vgmParser>				m_pParser;
			ATL::CComPtr<IMFMediaEventQueue>         m_pEventQueue;         // Event generator helper.
			ATL::CComPtr<IVGMSource>                  m_pSource;             // Parent media source
			ATL::CComPtr<IMFStreamDescriptor>         m_pStreamDescriptor;   // Stream descriptor for this stream.
			std::queue<ATL::CComPtr<IMFSample>>       m_sampleQueue;


		};

		OBJECT_ENTRY_AUTO(__uuidof(vgmStream), CvgmStream)
	}
}
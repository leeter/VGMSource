// vgmSource.h : Declaration of the CvgmSource

#pragma once
#include "resource.h"       // main symbols



#include "vgmcodec_i.h"
#include "vgmParser.h"
#include <gsl.h>

// CvgmSource
namespace vgmcodec{
	namespace io{
		class ATL_NO_VTABLE CvgmSource :
			public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
			public ATL::CComCoClass<CvgmSource, &CLSID_vgmSource>,
			public IVGMSource
		{
		public:
			CvgmSource(void)
				:m_IsShutdown(false)
			{
			}

			DECLARE_REGISTRY_RESOURCEID(IDR_VGMSOURCE)

			DECLARE_NOT_AGGREGATABLE(CvgmSource)

			BEGIN_COM_MAP(CvgmSource)
				COM_INTERFACE_ENTRY(IVGMSource)
				COM_INTERFACE_ENTRY(IMFMediaSource)
				COM_INTERFACE_ENTRY(IMFMediaEventGenerator)
			END_COM_MAP()



			DECLARE_PROTECT_FINAL_CONSTRUCT()

			HRESULT FinalConstruct() noexcept;

			void FinalRelease() noexcept
			{
			}

		public:
			// IMFMediaSource
			IFACEMETHOD(GetCharacteristics)( 
				/* [out] */ __RPC__out DWORD *pdwCharacteristics) noexcept;
			IFACEMETHOD(CreatePresentationDescriptor)( 
				/* [annotation][out] */ 
				__out  IMFPresentationDescriptor **ppPresentationDescriptor) noexcept;
			IFACEMETHOD(Start)( 
				/* [in] */ __RPC__in_opt IMFPresentationDescriptor *pPresentationDescriptor,
				/* [unique][in] */ __RPC__in_opt const GUID *pguidTimeFormat,
				/* [unique][in] */ __RPC__in_opt const PROPVARIANT *pvarStartPosition);
			IFACEMETHOD(Stop)(void) noexcept;
			IFACEMETHOD(Pause)(void) noexcept;
			IFACEMETHOD(Shutdown)(void)noexcept;

			// IMFMediaEventGenerator
			IFACEMETHOD(BeginGetEvent)(IMFAsyncCallback* pCallback, IUnknown* punkState) noexcept;
			IFACEMETHOD(EndGetEvent)(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent) noexcept;
			IFACEMETHOD(GetEvent)(DWORD dwFlags, IMFMediaEvent** ppEvent) noexcept;
			IFACEMETHOD(QueueEvent)(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue) noexcept;

			// IVGMSource
			IFACEMETHOD(GetPlayState)(REF_VGM_PLAY_STATE pState) noexcept;
			IFACEMETHOD(Open)(IMFByteStream * pOutputStream) noexcept;

		private:
			HRESULT CheckShutdown() const noexcept
			{
				if (m_IsShutdown)
				{
					return MF_E_SHUTDOWN;
				}
				else
				{
					return S_OK;
				}
			}

			HRESULT ValidatePresentationDescriptor(gsl::not_null<IMFPresentationDescriptor*> pPresentationDescriptor);
			HRESULT QueueNewStreamEvent(gsl::not_null<IMFPresentationDescriptor*> pPD);
			HRESULT createStream(ATL::CComPtr<IMFStreamDescriptor> const& pPD);
			
			bool                        m_IsShutdown;               // Flag to indicate if Shutdown() method was called.
			VGM_PLAY_STATE                       m_state;                    // Current state (running, stopped, paused)

			ATL::CComPtr<IPropertyStoreCache> 		m_pPCache;
			ATL::CComPtr<IMFMediaEventQueue>          m_pEventQueue;             // Event generator helper
		    ATL::CComPtr<IMFPresentationDescriptor>   m_pPresentationDescriptor; // Default presentation
			ATL::CComPtr<IVGMStream>              m_pStream;
			std::shared_ptr<vgmcodec::parser::vgmParser>                m_pParser;

			HRESULT     CreatePresentationDescriptor(void);

		};

		OBJECT_ENTRY_AUTO(__uuidof(vgmSource), CvgmSource)
	}
}
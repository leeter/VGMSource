#pragma once
#include "VgmSample.h"
#include <mfobjects.h>
#include "vgmheader.h"
namespace vgmcodec{
	namespace parser{
		class vgmParser
		{
		public:
			vgmParser(ATL::CComPtr<IMFByteStream> const&);
			bool getNextSample(vgmcodec::sample::VgmSample&);
			vgmcodec::format::vgmHeader getHeader() const _NOEXCEPT { return this->m_Header; }
		private:
			vgmParser(){};

			DWORD headerSize;
			int loopOffset;
			int loopCount;
			ATL::CComPtr<IMFByteStream> m_dataStream;
			vgmcodec::format::vgmHeader m_Header;
			std::vector<BYTE> sampleBuffer;
			std::vector<BYTE>::const_iterator m_SampleOffsetLocation;
		};
	}
}
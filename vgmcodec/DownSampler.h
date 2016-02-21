#pragma once
#include "resampler.h"
namespace vgmcodec{
	namespace transform{
		namespace resampling{
			class DownSampler :
				public Resampler
			{
			public:
				IFACEMETHOD(Write)(vgmcodec::sample::VgmSample sample);
				IFACEMETHOD(Render)(vgmcodec::sample::wsample_t * __restrict buffer, UINT32 count);
				
				DownSampler(void);
				~DownSampler(void);
			};
		}
	}
}


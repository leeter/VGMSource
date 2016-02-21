#pragma once
#include <memory>
#include "IChip.h"

namespace vgmcodec{
	namespace transform{
		namespace resampling{
			std::shared_ptr<vgmcodec::transform::chips::IChip> CreateResamplerForChip(std::shared_ptr<vgmcodec::transform::chips::IChip> const & toResample);	
		}
	}
}


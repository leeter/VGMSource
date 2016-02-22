#ifndef _ICHIP_H
#define _ICHIP_H
#pragma once
#include <cstdint>
#include <gsl.h>
#include "VgmSample.h"

namespace vgmcodec{
	namespace transform{
		namespace chips{
			class __declspec (novtable) IChip
			{
			public:
				virtual void Write(vgmcodec::sample::VgmSample const & sample) = 0;
				virtual void Render(gsl::span<vgmcodec::sample::wsample_t> buffer) = 0;
				virtual std::uint32_t GetDACClock(void) const noexcept = 0;
				virtual std::uint32_t GetSampleRate(void) const noexcept = 0;
				virtual ~IChip() noexcept = default;
			};
		}
	}
}

#endif

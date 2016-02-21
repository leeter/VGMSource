#pragma once
#include "ichip.h"

namespace vgmcodec{
	namespace transform{
		namespace chips{
			class sn76496 :
				public IChip
			{
			public:
				sn76496(std::uint32_t uDACClock, std::uint16_t shiftRegWidth = 16, std::uint8_t feedback = 0x0009, std::uint8_t flags = 0);
				void Write(vgmcodec::sample::VgmSample const & sample) override;
				void Render(gsl::span<vgmcodec::sample::wsample_t> buffer) override;
				std::uint32_t GetDACClock(void) const noexcept override { return this->m_uDacClock; }
				std::uint32_t GetSampleRate(void) const noexcept override { return this->m_uSampleRate; };

			private:
				const std::uint32_t m_uDacClock;
				const std::int32_t	clockDivider;
				const std::uint32_t m_uSampleRate;
				const std::uint16_t	freqLimit;
				const bool	freq0maxed;
				const bool	stereo;
				const std::int32_t	feedbackMask;
				std::uint8_t m_channelMask;
				int m_lastRegister;
				std::uint32_t	rng;
				std::int32_t	regs[8];
				std::int32_t	period[4];
				std::int32_t	count[4];
				std::int32_t	output[4];
				std::uint32_t   MuteMsk[4];				
				
				std::int16_t	volumeTable[16];
				std::int32_t	volume[4];
				std::int32_t	whiteNoise1;
				std::int32_t	whiteNoise2;
				
				void WriteReg(std::uint8_t bData);
			};
		}
	}
}


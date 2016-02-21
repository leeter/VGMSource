#include "StdAfx.h"
#include "Resampler.h"
#include <stdexcept>

#define FIXPNT_BITS		11
#define FIXPNT_FACT		(1 << FIXPNT_BITS)
#if (FIXPNT_BITS <= 11)
typedef ::std::uint32_t	SLINT;	// 32-bit is a lot faster
#else
typedef UINT64	SLINT;
#endif
#define FIXPNT_MASK		(FIXPNT_FACT - 1)

//#define getfriction(x)	((x) & FIXPNT_MASK)
//#define getnfriction(x)	((FIXPNT_FACT - (x)) & FIXPNT_MASK)
//#define fpi_floor(x)	((x) & ~FIXPNT_MASK)
//#define fpi_ceil(x)		((x + FIXPNT_MASK) & ~FIXPNT_MASK)
//#define fp2i_floor(x)	((x) / FIXPNT_FACT)
//#define fp2i_ceil(x)	((x + FIXPNT_MASK) / FIXPNT_FACT)
using namespace vgmcodec::transform::resampling;
namespace {
	SLINT inline constexpr getfriction(SLINT x){ return x & FIXPNT_MASK; }
	//SLINT inline fpi_floor(SLINT x){ return x & ~FIXPNT_MASK; }
	SLINT inline constexpr getnfriction(SLINT x){ return (FIXPNT_FACT - x) & FIXPNT_MASK; }

	SLINT inline constexpr fp2i_floor(SLINT x){ return x / FIXPNT_FACT; }

	SLINT inline constexpr fp2i_ceil(SLINT x){ return (x + FIXPNT_MASK) / FIXPNT_FACT; }

	class __declspec (novtable) Resampler :
		public vgmcodec::transform::chips::IChip
	{
	public:
		void Write(vgmcodec::sample::VgmSample const & sample) override
		{
			this->m_pChip->Write(sample);
		}
		std::uint32_t GetDACClock(void) const noexcept override { return this->m_uDACClock; };
		std::uint32_t GetSampleRate(void) const noexcept override
		{
			return m_sampleRate;
		};

	private:
		const std::uint32_t m_uDACClock;
		const std::uint32_t m_chipSampleRate;
		const std::uint32_t m_sampleRate;
		const std::shared_ptr<IChip> m_pChip;

	protected:
		Resampler(std::shared_ptr<IChip> const & toResample)
			:m_uDACClock(0), m_pChip(toResample), m_sampleRate(VGM_SAMPLE_RATE), m_chipSampleRate(toResample->GetSampleRate())
		{}
		virtual ~Resampler(void) = default;

		// in this particular case RVO is not possible so just return a const ref
		std::shared_ptr<IChip> const& GetChip(void) const { return this->m_pChip; };
	};

	class Downsampler : public Resampler
	{
	public:
		Downsampler(std::shared_ptr<IChip> const & toDownSample)
			:Resampler(toDownSample),
			m_sampleLast(0),
			m_sampleNext(0),
			m_SampleCount(0),
			lastSample({ 0, 0 }){}

		void Render(gsl::span<vgmcodec::sample::wsample_t> buffer) override
		{
			if (buffer.empty())
				throw std::invalid_argument("buffer cannot be null");
			if (buffer.size())
				return;

			std::array<vgmcodec::sample::wsample_t, 0x100> samples;
			samples.fill(vgmcodec::sample::wsample_t{ 0, 0 });
			//ZeroMemory(std::addressof(samples[0]), ARRAYSIZE(samples));

			std::int64_t TempSmpL = 0, TempSmpR = 0;
			vgmcodec::sample::wsample_t finalSample = { 0, 0 };
			const std::uint32_t outputSampleRate = this->GetSampleRate();
			const std::uint32_t chipSampleRate = this->GetChip()->GetSampleRate();
			SLINT InPosL = static_cast<SLINT>(FIXPNT_FACT * (this->m_SampleCount + buffer.size()) * chipSampleRate / outputSampleRate);

			this->m_sampleNext = static_cast<std::uint32_t>(fp2i_ceil(InPosL));
			std::uint32_t toRender = this->m_sampleNext - this->m_sampleLast;

			samples[0] = this->lastSample;
			auto mainSpan = gsl::as_span(samples);
			this->GetChip()->Render(mainSpan.subspan(1, toRender));

			std::uint32_t InBase = FIXPNT_FACT + static_cast<std::uint32_t>(
				(FIXPNT_FACT * static_cast<UINT64>(this->m_SampleCount) * chipSampleRate / outputSampleRate)
				- this->m_sampleLast * FIXPNT_FACT);
			std::uint32_t InPosNext = InBase;
			std::uint32_t InPre = 0;
			for (std::uint32_t outPosition = 0; outPosition < buffer.size(); ++outPosition)
			{
				std::uint32_t InPos = InPosNext;

				InPosNext = InBase + static_cast<std::uint32_t>(FIXPNT_FACT * (outPosition + 1) * chipSampleRate / outputSampleRate);

				// first frictional Sample
				std::uint32_t SmpFrc = getnfriction(InPos);
				if (SmpFrc)
				{
					InPre = fp2i_floor(InPos);
					TempSmpL = static_cast<std::int64_t>(samples[InPre].left) * SmpFrc;
					TempSmpR = static_cast<std::int64_t>(samples[InPre].right) * SmpFrc;
				}
				else
				{
					TempSmpL = TempSmpR = 0x00;
				}
				std::int32_t SmpCnt = SmpFrc;

				// last frictional Sample
				SmpFrc = getfriction(InPosNext);
				InPre = fp2i_floor(InPosNext);
				if (SmpFrc)
				{
					TempSmpL += static_cast<std::int64_t>(samples[InPre].left) * SmpFrc;
					TempSmpR += static_cast<std::int64_t>(samples[InPre].right) * SmpFrc;
					SmpCnt += SmpFrc;
				}

				// whole Samples in between
				std::uint32_t InNow = fp2i_ceil(InPos);
				SmpCnt += (InPre - InNow) * FIXPNT_FACT;
				while (InNow < InPre)
				{
					TempSmpL += static_cast<std::int64_t>(samples[InNow].left) * FIXPNT_FACT;
					TempSmpR += static_cast<std::int64_t>(samples[InNow].right) * FIXPNT_FACT;
					InNow++;
				}
				finalSample.left += static_cast<std::int32_t>(TempSmpL * 512 / SmpCnt);
				finalSample.right += static_cast<std::int32_t>(TempSmpR * 512 / SmpCnt);
				buffer[outPosition] = finalSample;
			}
			this->lastSample = samples[InPre];
			this->m_sampleLast = this->m_sampleNext;
			this->m_SampleCount += static_cast<std::uint32_t>(buffer.size());
			if (this->m_sampleLast >= chipSampleRate)
			{
				this->m_sampleLast -= chipSampleRate;
				this->m_sampleNext -= chipSampleRate;
				this->m_SampleCount -= outputSampleRate;
			}
			return;
		}
	private:
		std::uint32_t			m_sampleLast;
		std::uint32_t			m_sampleNext;
		std::uint32_t			m_SampleCount;
		vgmcodec::sample::wsample_t lastSample;
	};

}

std::shared_ptr<vgmcodec::transform::chips::IChip> vgmcodec::transform::resampling::CreateResamplerForChip(std::shared_ptr<vgmcodec::transform::chips::IChip> const & toResample)
{
	if (toResample == nullptr)
		throw std::invalid_argument("toResample may not be a nullptr");

	if (toResample->GetSampleRate() > VGM_SAMPLE_RATE)
	{
		return std::make_shared<Downsampler>(toResample);
	}
	// do nothing just pass back the chip itself
	return toResample;
}

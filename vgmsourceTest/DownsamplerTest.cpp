#include "stdafx.h"
#include "CppUnitTest.h"
#include "../vgmcodec/Resampler.h"
#include <type_traits>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace vgmcodec::transform::resampling;
using namespace vgmcodec::transform::chips;

namespace{

	class DownsampleChipMock : public IChip
	{
	public:
		DownsampleChipMock()
			:mb_writeCalled(false)
		{}
		void Write(vgmcodec::sample::VgmSample const & sample) override
		{
			this->mb_writeCalled = true;
		}
		void Render(vgmcodec::sample::wsample_t buffer[], size_t count) override
		{

		}
		std::uint32_t GetDACClock(void) const throw() override { return 44100 * 2; }
		std::uint32_t GetSampleRate(void) const throw() override { return 44100 * 2; }
		bool writeCalled() const throw() { return this->mb_writeCalled; }
	private:
		bool mb_writeCalled;

	};
}

namespace vgmsourceTest
{
	TEST_CLASS(DownsamplerTest)
	{
	public:
		
		TEST_METHOD(CreateDownsamplerTest)
		{
			std::shared_ptr<IChip> mockChip = std::make_shared<DownsampleChipMock>();
			std::shared_ptr<IChip> chip = vgmcodec::transform::resampling::CreateResamplerForChip(mockChip);
			Assert::IsNotNull(chip.get());
			Assert::AreNotEqual(mockChip->GetDACClock(), chip->GetDACClock());
			Assert::AreEqual(VGM_SAMPLE_RATE, chip->GetSampleRate());
		}

		TEST_METHOD(writeTest)
		{
			std::shared_ptr<DownsampleChipMock> mockChip = std::make_shared<DownsampleChipMock>();
			std::shared_ptr<IChip> chip = vgmcodec::transform::resampling::CreateResamplerForChip(mockChip);
			vgmcodec::sample::VgmSample sample = { 0 };
			chip->Write(sample);
			Assert::IsTrue(mockChip->writeCalled(), L"write should have been called in the inner chip");
		}

		TEST_METHOD(renderTest)
		{
			std::shared_ptr<DownsampleChipMock> mockChip = std::make_shared<DownsampleChipMock>();
			std::shared_ptr<IChip> chip = vgmcodec::transform::resampling::CreateResamplerForChip(mockChip);
			vgmcodec::sample::wsample_t sample = { 0 };
			chip->Render(std::addressof(sample), 1);
		}

		TEST_METHOD(invalidBufferTest)
		{
			std::shared_ptr<DownsampleChipMock> mockChip = std::make_shared<DownsampleChipMock>();
			std::shared_ptr<IChip> chip = vgmcodec::transform::resampling::CreateResamplerForChip(mockChip);
			try{
				chip->Render(nullptr, 1);
				Assert::Fail(L"invalid argument not thrown");
			}
			catch (std::invalid_argument)
			{
			}
		}
	};
}
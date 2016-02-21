#include "stdafx.h"
#include "CppUnitTest.h"
#include "../vgmcodec/Resampler.h"

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
		std::uint32_t GetDACClock(void) const noexcept override { return 44100 * 2; }
		std::uint32_t GetSampleRate(void) const noexcept override { return 44100 * 2; }
		bool writeCalled() const noexcept { return this->mb_writeCalled; }
	private:
		bool mb_writeCalled;

	};

	class ResampleChipMock : public IChip
	{
	public:
		ResampleChipMock()
			:mb_writeCalled(false)
		{}
		void Write(vgmcodec::sample::VgmSample const & sample) override
		{
			this->mb_writeCalled = true;
		}
		void Render(vgmcodec::sample::wsample_t buffer[], size_t count) override
		{

		}
		std::uint32_t GetDACClock(void) const throw() override { return 44100; }
		std::uint32_t GetSampleRate(void) const throw() override { return 44100; }
		bool writeCalled() const throw() { return this->mb_writeCalled; }
	private:
		bool mb_writeCalled;

	};
}
template<> static std::wstring Microsoft::VisualStudio::CppUnitTestFramework::ToString<::std::shared_ptr<IChip> >(const ::std::shared_ptr<IChip> & t) { RETURN_WIDE_STRING(t.get()); }
namespace vgmsourceTest
{
	TEST_CLASS(ResamplerTest)
	{
	public:
		
		TEST_METHOD(CreateResamplerForChip_BadParameterTest)
		{
			Assert::ExpectException<std::invalid_argument, void>([](){
				vgmcodec::transform::resampling::CreateResamplerForChip(nullptr);
			});
		}

		TEST_METHOD(CreateResamplerForChip_ExactRateTest)
		{
			::std::shared_ptr<IChip> mockChip = ::std::make_shared<ResampleChipMock>();
			::std::shared_ptr<IChip> chip = ::vgmcodec::transform::resampling::CreateResamplerForChip(mockChip);
			Assert::AreEqual(mockChip, chip, L"The chips must be equal");
		}
	};
}
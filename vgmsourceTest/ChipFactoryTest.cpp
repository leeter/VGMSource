#include "stdafx.h"
#include "CppUnitTest.h"
#include "../vgmcodec/ChipFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace vgmcodec::format;
using namespace vgmcodec::transform;


template<> static std::wstring Microsoft::VisualStudio::CppUnitTestFramework::ToString<VGM_VERSION>(const VGM_VERSION & t) { RETURN_WIDE_STRING(t); }

namespace vgmsourceTest
{
	
	TEST_CLASS(ChipFactoryTest)
	{
	public:
		BEGIN_TEST_METHOD_ATTRIBUTE(factory_v101Test)
			TEST_DESCRIPTION(L"Tests the construction of a version 1.01 factory")
		END_TEST_METHOD_ATTRIBUTE()
		TEST_METHOD(factory_v101Test)
		{
			::vgmcodec::format::vgmHeader header = { /* ident */ 0, /*eof*/ 0, VGM_VERSION::VGM_VERSION_101, 3579545 };
			std::unique_ptr<CChipFactory> factory = CChipFactory::createFactory(header);

			Assert::AreEqual(VGM_VERSION::VGM_VERSION_101, factory->getFactoryVersion());
		}

		TEST_METHOD(factory_invalidVersion)
		{
			Assert::ExpectException<std::exception, void>([](){
				::vgmcodec::format::vgmHeader header = { /* ident */ 0, /*eof*/ 0, -1, 3579545 };
				std::unique_ptr<CChipFactory> factory = CChipFactory::createFactory(header);
			});
		}

	};
}
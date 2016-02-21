#pragma once
#include "vgmheader.h"
#include "IChip.h"
#include <vector>

namespace vgmcodec{
	namespace transform{
		class __declspec(novtable) CChipFactory
		{
		public:
			static std::unique_ptr<CChipFactory> createFactory(vgmcodec::format::vgmHeader const&);
			virtual ~CChipFactory();
			virtual std::vector<std::shared_ptr<vgmcodec::transform::chips::IChip> > getChips() = 0;
			virtual vgmcodec::format::VGM_VERSION getFactoryVersion() const _NOEXCEPT = 0;

		protected:
			CChipFactory();
		private:
			CChipFactory(CChipFactory const&) = delete;
			CChipFactory(CChipFactory&&) = delete;
			CChipFactory& operator=(CChipFactory const&) = delete;
			CChipFactory& operator=(CChipFactory&&) = delete;
		};
	}
}


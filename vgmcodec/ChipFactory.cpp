#include "stdafx.h"
#include "ChipFactory.h"
#include "Resampler.h"
#include "sn76496.h"
using namespace vgmcodec::transform;
using namespace vgmcodec::format;

namespace{
	class v101Factory : public CChipFactory
	{
	public:
		v101Factory(vgmcodec::format::vgmHeader const& header)
			:header(header)
		{

		}
		std::vector<std::shared_ptr<vgmcodec::transform::chips::IChip> > getChips() override
		{
			std::vector<std::shared_ptr<vgmcodec::transform::chips::IChip> > chips;
			if (this->header.SN76489clock){
				chips.push_back(
					resampling::CreateResamplerForChip(
					std::make_shared<vgmcodec::transform::chips::sn76496>(this->header.SN76489clock)));
			}

			return chips;
		}

		vgmcodec::format::VGM_VERSION getFactoryVersion() const _NOEXCEPT override
		{
			return VGM_VERSION::VGM_VERSION_101;
		}
	protected:
		vgmcodec::format::vgmHeader header;
	};
}

CChipFactory::CChipFactory()
{
}


CChipFactory::~CChipFactory() _NOEXCEPT
{
}

std::unique_ptr<CChipFactory> CChipFactory::createFactory(vgmcodec::format::vgmHeader const& header)
{
	if (header.version == VGM_VERSION::VGM_VERSION_100 || header.version == VGM_VERSION::VGM_VERSION_101){
		return std::make_unique<v101Factory>(header);
	}
	throw std::invalid_argument("invalid header");
}

#ifndef _VGMHEADER_H
#define _VGMHEADER_H

#ifdef _MSC_VER
#pragma once
#endif
#define MAKE_VGM_VERSION(A,B) (((A) & 0xFF) << 8 | ((B) & 0xFF))
#include <cstdint>
namespace vgmcodec{
	namespace format{
		struct vgmHeader{
			std::uint32_t ident;
			std::uint32_t eof;
			std::uint32_t version;
			std::uint32_t SN76489clock;
			std::uint32_t YM2413clock;
			std::uint32_t GD3offset;
			std::uint32_t sampleCount;
			std::uint32_t loopOffset;
			std::uint32_t loopSampleCount;
			std::uint32_t recordingRate;
			std::uint16_t SN76489feedback : 16;
			std::uint8_t SN76489shiftRegisterWidth : 8;
			std::uint8_t SN76489Flags : 8;
			std::uint32_t YM2612clock;
			std::uint32_t YM2151clock;
			std::uint32_t dataOffset;
			std::uint32_t SegaPCMclock;
			std::uint32_t SegaPCMinterfaceRegister;
			std::uint32_t RF5C68clock;
			std::uint32_t YM2203clock;
			std::uint32_t YM2608clock;
			std::uint32_t YM2610_YM2610Bclock;
			std::uint32_t YM3812clock;
			std::uint32_t YM3526clock;
			std::uint32_t Y8950clock;
			std::uint32_t YMF262clock;
			std::uint32_t YMF278Bclock;
			std::uint32_t YMF271clock;
			std::uint32_t YMZ280Bclock;
			std::uint32_t RF5C164clock;
			std::uint32_t PWMclock;
			std::uint32_t AY8910clock;
			std::uint8_t AY8910ChipType : 8;
			std::uint8_t AY8910Flags : 8;
			std::uint8_t YM2203_AY8910Flags : 8;
			std::uint8_t YM2608_AY8910Flags : 8;
			std::uint8_t volumeModifier : 8;
			std::uint8_t reserved : 8;
			std::uint8_t loopBase : 8;
			std::uint8_t loopModifier : 8;
		};

		enum VGM_VERSION : ::std::uint32_t{
			VGM_VERSION_100 = MAKE_VGM_VERSION(1, 0),
			VGM_VERSION_101 = MAKE_VGM_VERSION(1, 1),
			VGM_VERSION_110 = MAKE_VGM_VERSION(1, 10),
			VGM_VERSION_151 = MAKE_VGM_VERSION(1, 51),
			VGM_VERSION_160 = MAKE_VGM_VERSION(1,60)
		};
	}
}
#endif
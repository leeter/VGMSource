// This file is MAME Derivative and may not be used for commercial works

#include "StdAfx.h"
#include "sn76496.h"
namespace {
	constexpr int TONE0_FREQ = 0;
	constexpr int TONE1_FREQ = 2;
	constexpr int TONE2_FREQ = 4;
	constexpr int TONE0_VOL = 1;
	constexpr int TONE1_VOL = 3;
	constexpr int TONE2_VOL = 5;
	constexpr int NOISE_VOL = 7;
	constexpr int NOISE_FREQ_MODE = 6;
	constexpr std::uint8_t CLOCKDIVIDER_FLAG = 0x08;
	constexpr std::uint8_t FREQ0ISMAXED_FLAG = 0x01;
	constexpr int MAX_OUTPUT = 0x8000;
	constexpr int VOLUME_OFF = 0x0F;
	constexpr int NOISE_CHANNEL = 0x03;
	constexpr int MAX_OUTPUT_VOL = 0x0400;
	constexpr int NOISE_CHANNEL_MASK = 0x03;
	// indicates this is a latch operation and we should select a register
	constexpr std::uint8_t LATCH_DATA = 0x80;
	// mask to select the bits that indicate the register
	constexpr std::uint8_t LATCH_REG_SELECT_MASK = 0x70;
	constexpr std::uint8_t LATCH_DATA_MASK = 0x0F;
	constexpr std::uint8_t DATA_BYTE_MASK = 0x3F;
	constexpr std::uint32_t CHIP_CLOCK_MASK = 0x7FFFFFFF;
	// zeros lower four bits
	constexpr std::uint32_t LATCH_REG_SET_MASK = 0x3F0;
	// zeros upper six bits
	constexpr std::uint8_t LATCH_DATA_SET_MASK = 0x0F;
	constexpr int NOISE_CHANNEL_SLAVE_TONE2 = 0x03;
}

using namespace vgmcodec::transform::chips;

sn76496::sn76496(UINT32 uDACClock, std::uint16_t shiftRegWidth, std::uint8_t feedback, std::uint8_t flags)
	:m_channelMask(0xFF),
	m_uDacClock(uDACClock & CHIP_CLOCK_MASK),
	m_lastRegister(0),
	clockDivider(flags & CLOCKDIVIDER_FLAG ? 1 : 8),
	freq0maxed(flags & FREQ0ISMAXED_FLAG),
	stereo(true),
	feedbackMask(1 << (shiftRegWidth - 1)),
	rng(0x010000),
	m_uSampleRate((m_uDacClock /2) / clockDivider),
	freqLimit(static_cast<std::uint16_t>(uDACClock / (flags & CLOCKDIVIDER_FLAG ? 2.0 : 16.0) / VGM_SAMPLE_RATE))
{
	int ntap[2];
	int curtap = 0;
	for (int curbit = 0; curbit < 16; curbit++)
	{
		if (feedback & (1 << curbit))
		{
			ntap[curtap] = (1 << curbit);
			curtap++;
			if (curtap >= 2)
				break;
		}
	}
	while (curtap < 2)
	{
		ntap[curtap] = ntap[0];
		curtap++;
	}
	whiteNoise1 = ntap[0];
	whiteNoise2 = ntap[1];

	for(int i = 0; i < 4; ++i)
	{
		this->regs[i*2] = 1;
		this->regs[i*2 +1] = VOLUME_OFF;
	}

	for (int i = 0; i < 4; ++i)
	{
		this->period[i] = this->count[i] = this->output[i] = this->volume[i] = 0;
		this->MuteMsk[i] = ~0x00U;
	}

	constexpr double maxVol = static_cast<double>(MAX_OUTPUT / 4);
	double out = maxVol;
	for(int i = 0; i < 15; ++i)
	{
		this->volumeTable[i] = out > maxVol ? MAX_OUTPUT / 4 : static_cast<std::int16_t>(out + 0.5);
		out = out / 1.258925412;	/* = 10 ^ (2/20) = 2dB */
	}
	this->output[3] = this->rng & 1;
	this->volumeTable[15] = 0;
}

void sn76496::Write(vgmcodec::sample::VgmSample const& sample)
{
	switch(sample.opCode)
	{
	case 0x4F:
		this->m_channelMask = sample.bOps[0];
		break;
	case 0x50:
		return this->WriteReg(sample.bOps[0]);
	default:
		break;
	}
}

void sn76496::Render(gsl::span<vgmcodec::sample::wsample_t> buffer)
{
	INT32 left = 0, right = 0;
	INT32 vol[4];
	INT32 ggst[2];

	for (int i = 0; i < 3; ++i)
	{
		if (this->period[i] || this->volume[i])
		{
			left = 1;
			break;
		}
	}
	left |= this->volume[NOISE_CHANNEL];
	if (!left)
	{
		std::fill(buffer.begin(), buffer.end(), vgmcodec::sample::wsample_t{});
		return;
	}
	ggst[0] = 0x01;
	ggst[1] = 0x01;

	if(!count)
		throw std::invalid_argument("count cannot be zero");
	/*if (this->cyclesToReady)
	{
	this->cyclesToReady = this->cyclesToReady - 1;
	}*/


	// each call to render should take 
	for(auto & sample : buffer)
	{
		// handle tone channels
		for(int i = 0; i < 3; ++i)
		{
			this->count[i]--;
			if(this->count[i] <= 0)
			{
				this->output[i] ^= 1;
				this->count[i] = this->period[i];
			}
		}

		// handle noise channel
		this->count[3]--;

		if(this->count[3] <= 0)
		{
			// if this->regs[NOISE_FREQ_MODE] & 4 is nonzero, both taps are enabled
			// if this->regs[NOISE_FREQ_MODE] & 4 is zero, the lower tap, whitenoise2, is held at 0
			bool tapsOn = (this->rng & this->whiteNoise1 ? true : false) != ((this->rng & this->whiteNoise2) && this->regs[NOISE_FREQ_MODE] & 4);
			this->rng = this->rng >> 1;
			if(tapsOn)
			{
				this->rng = this->rng | this->feedbackMask;
			}

			this->output[NOISE_CHANNEL] = this->rng & 1;

			this->count[NOISE_CHANNEL] = this->period[NOISE_CHANNEL];
		}
		// end handle noise channel
		left = right = 0;
		for(int i = 0; i < 4; ++i)
		{
			vol[i] = this->output[i] ? +1 : -1;
			if(i != NOISE_CHANNEL && (this->period[i] <= this->freqLimit && this->period[i] > 1))
			{
				vol[i] = 0;
			}

			vol[i] = vol[i] & this->MuteMsk[i];

			if(this->stereo)
			{
				ggst[0] = this->m_channelMask & (0x10 << i);
				ggst[1] = this->m_channelMask & (0x01 << i);
			}

			if(this->period[i] > 1 || i == NOISE_CHANNEL)
			{
				int outVol = vol[i] * this->volume[i];
				left = ggst[0] ?  left + outVol : left;
				right = ggst[1] ? right + outVol : right;
			}
			else
			{

				left = ggst[0] ? (left + this->volume[i]) : left;
				right = ggst[1] ? (right + this->volume[i]) : right;
			}

		}

		sample.left = left >> 1;
		sample.right = right >> 1;
	}

	return;

}

void sn76496::WriteReg(::std::uint8_t bData)
{
	int reg;
	if(bData & LATCH_DATA){
		// shift over four bits to the lower four bits of the register
		reg = (bData & LATCH_REG_SELECT_MASK) >> 4;
		this->m_lastRegister = reg;
		// set the lower four bits of the register
		this->regs[reg] = (this->regs[reg] & LATCH_REG_SET_MASK) | bData & LATCH_DATA_MASK;

	}
	else{
		reg = this->m_lastRegister;
	}

	int pSelect = reg/2;
	switch(reg)
	{
	case TONE0_FREQ: /* tone 0 : frequency */
	case TONE1_FREQ: /* tone 1 : frequency */
	case TONE2_FREQ: /* tone 2 : frequency */
		// prevent a divide by zero situation
		{
			/* Latch data set, sets the upper 6 bits of the tone register
			*/
			if((bData & LATCH_DATA) == 0)
				this->regs[reg] = (this->regs[reg] & LATCH_DATA_SET_MASK) | ((bData & DATA_BYTE_MASK) << 4) ;

			// TODO: figure out what the 0x400 is
			this->period[pSelect] = ( this->regs[reg] != 0 || !this->freq0maxed) ? this->regs[reg] : MAX_OUTPUT_VOL;

			if(reg == TONE2_FREQ && (this->regs[NOISE_FREQ_MODE] & NOISE_CHANNEL_MASK) == NOISE_CHANNEL_SLAVE_TONE2)
				this->period[NOISE_CHANNEL] = 2 * this->period[2];
		}
		break;
	case TONE0_VOL: /* tone 0 : volume */
	case TONE1_VOL: /* tone 1 : volume */
	case TONE2_VOL: /* tone 2 : volume */
	case NOISE_VOL: /* noise  : volume */
		this->volume[pSelect] = this->volumeTable[bData & LATCH_DATA_MASK];
		if((bData & LATCH_DATA) == 0)
			this->regs[reg] = (this->regs[reg] & LATCH_REG_SET_MASK) | bData & LATCH_DATA_MASK;
		break;
	case NOISE_FREQ_MODE:
		{

			if((bData & LATCH_DATA) == 0)
				this->regs[reg] = (this->regs[reg] & LATCH_REG_SET_MASK) | bData & LATCH_DATA_MASK;

			std::int32_t currentNoise = this->regs[NOISE_FREQ_MODE];
			this->period[3] = (currentNoise & NOISE_CHANNEL_MASK) == NOISE_CHANNEL_SLAVE_TONE2 ? 2 * this->period[2] : (1 << (5 + (currentNoise & NOISE_CHANNEL_MASK)));
			this->rng = this->feedbackMask;
		}
		break;

	default:
		break;
	}
}
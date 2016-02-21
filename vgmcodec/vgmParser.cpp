#include "StdAfx.h"
#include "vgmParser.h"
#include <exception>

using namespace vgmcodec::parser;
using namespace vgmcodec::format;
using namespace vgmcodec::sample;

vgmParser::vgmParser(const ATL::CComPtr<IMFByteStream> & inputStream)
:vgmParser() // delegate the constructor to ensure that the destructor will be called if something bad happens
{
	
	static_assert(sizeof(vgmHeader) == 0x80, "vgmHeader size is incorrect");
	if (!inputStream)
		_com_issue_error(E_INVALIDARG);
	QWORD bufferSize = 0;
	_com_util::CheckError(inputStream->GetLength(std::addressof(bufferSize)));
	
	this->sampleBuffer.resize(bufferSize);

	_com_util::CheckError(inputStream->SetCurrentPosition(0));
	ULONG read = 0;
	_com_util::CheckError(
		inputStream->Read(
			reinterpret_cast<BYTE*>(std::addressof(this->m_Header)),
			sizeof(vgmHeader),
			std::addressof(read)));
	if (read != sizeof(vgmHeader)){
		_com_issue_error(E_ABORT);
	}
	
	if (this->m_Header.ident != FCC('Vgm ')){
		_com_issue_error(E_ABORT);
	}
	this->m_dataStream = inputStream;
	this->headerSize = this->m_Header.version > VGM_VERSION::VGM_VERSION_151 ? 0x80 : 0x40;
	ULONG gd3Location = this->m_Header.GD3offset + 0x14; // WTF is 0x14?
	_com_util::CheckError(this->m_dataStream->SetCurrentPosition(headerSize));

	
	_com_util::CheckError(this->m_dataStream->Read(std::addressof(this->sampleBuffer[0]), gd3Location - headerSize, std::addressof(read)));
	this->m_SampleOffsetLocation = this->sampleBuffer.cbegin();
	this->loopOffset = this->m_Header.loopOffset + 0x0000001C - this->headerSize;
	this->loopOffset = this->loopOffset > 0 ? this->loopOffset : 0;
	this->loopCount = this->m_Header.version > VGM_VERSION::VGM_VERSION_151 ? (this->m_Header.loopModifier - this->m_Header.loopBase) / 0x10 : 1;
	this->loopCount = this->loopCount >= 0 ? this->loopCount : 0;
}

bool vgmParser::getNextSample(vgmcodec::sample::VgmSample& sample){
	ZeroMemory(std::addressof(sample), sizeof(VgmSample));
// fugly but in this case actually the best choice
getOpCode:
	sample.opCode = *m_SampleOffsetLocation;
	++m_SampleOffsetLocation;
	std::vector<BYTE>::difference_type toRead = 0;
	switch (sample.opCode)
	{
		// single operand single byte commands
	case 0x4F:
	case 0x50:
		toRead = 1;
		break;
	case 0x51: // YM2413, write value dd to register aa
	case 0x52: // YM2612 port 0, write value dd to register aa
	case 0x53: // YM2612 port 1, write value dd to register aa
	case 0x54: // YM2151, write value dd to register aa
	case 0x55: // YM2203, write value dd to register aa
	case 0x56: // YM2608 port 0, write value dd to register aa
	case 0x57: // YM2608 port 1, write value dd to register aa
	case 0x58: // YM2610 port 0, write value dd to register aa
	case 0x59: // YM2610 port 1, write value dd to register aa
	case 0x5A: // YM3812, write value dd to register aa
	case 0x5B: // YM3526, write value dd to register aa
	case 0x5C: // Y8950, write value dd to register aa
	case 0x5D: // YMZ280B, write value dd to register aa
	case 0x5E: // YMF262 port 0, write value dd to register aa
	case 0x5F: // YMF262 port 1, write value dd to register aa
	case 0x61: // Wait n samples, n can range from 0 to 65535 (approx 1.49 seconds). Longer pauses than this are represented by multiple
	case 0xA0: // AY8910, write value dd to register aa
	case 0xB0: // RF5C68, write value dd to register aa
	case 0xB1: // RF5C164, write value dd to register aa
	case 0xB2: // PWM, write value ddd to register a (d is MSB, dd is LSB)
	case 0xB3: // GameBoy DMG, write value dd to register aa
	case 0xB4: // NES APU, write value dd to register aa
	case 0xB5: // MultiPCM, write value dd to register aa
	case 0xB6: // uPD7759, write value dd to register aa
	case 0xB7: // OKIM6258, write value dd to register aa
	case 0xB8: // OKIM6295, write value dd to register aa
	case 0xB9: // HuC6280, write value dd to register aa
	case 0xBA: // K053260, write value dd to register aa
	case 0xBB: // Pokey, write value dd to register aa
		toRead = 2;
		break;
	case 0xC0: // Sega PCM, write value dd to memory offset aaaa
	case 0xC1: // RF5C68, write value dd to memory offset aaaa
	case 0xC2: // RF5C164, write value dd to memory offset aaaa
	case 0xC3: // MultiPCM, write set bank offset aaaa to channel cc
	case 0xC4: // QSound, write value mmll to register rr (mm - data MSB, ll - data LSB)
	case 0xD0: // YMF278B port pp, write value dd to register aa
	case 0xD1: // YMF271 port pp, write value dd to register aa
	case 0xD2: // SCC1 port pp, write value dd to register aa
	case 0xD3: // K054539 write value dd to register ppaa
	case 0xD4: // C140 write value dd to register ppaa
		toRead = 3;
		break;
	case 0xE0: // seek to offset dddddddd (Intel byte order) in PCM data bank
		toRead = 4;
		break;
	case 0x66: // end of data, hopefully the below should be unnecessary...
		if (!this->loopCount){ return false; }
		this->m_SampleOffsetLocation = this->sampleBuffer.cbegin();
		this->m_SampleOffsetLocation += this->loopOffset;
		--this->loopCount;
		goto getOpCode;
	case 0x62: //wait 735 samples(60th of a second), a shortcut for 0x61 0xdf 0x02
	case 0x63: // wait 882 samples (50th of a second), a shortcut for 0x61 0x72 0x03
	case 0x70: // 0x7n  : wait n+1 samples, n can range from 0 to 15.
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
	case 0x78:
	case 0x79:
	case 0x7A:
	case 0x7B:
	case 0x7C:
	case 0x7D:
	case 0x7F:
		break;
	default: // do nothing no operands
		throw ::std::domain_error("invalid opcode");
	}

	std::copy(this->m_SampleOffsetLocation, this->m_SampleOffsetLocation + toRead, sample.bOps);

	this->m_SampleOffsetLocation += toRead;

	return true;
}

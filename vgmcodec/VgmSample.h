#pragma once
#include <cstdint>
#define CHANNEL_COUNT 2
#define BITS_PERSAMPLE sizeof(vgmcodec::sample::codec_sample_t) * 8
#define OUTPUT_SAMPLES_PER_SEC 44100
#define OUTPUT_BLOCK_ALIGN (CHANNEL_COUNT * BITS_PERSAMPLE) / 8
#define AVG_OUTPUT_BYTES_PER_SEC(sampleRate) sampleRate * OUTPUT_BLOCK_ALIGN
#define VGM_SAMPLE_RATE 44100U
namespace vgmcodec {
	namespace sample {
		using codec_sample_t = std::int16_t;
		using wcodec_sample_t = std::int32_t;

		struct VgmSample {
			std::uint8_t opCode;
			std::uint8_t dataType;
			union{
				std::uint8_t bOps[4];
				std::uint16_t sOps[2];
				std::uint32_t uOP;
				struct streamdata{
					std::uint8_t streamId;
					union {
						struct startData{
							union{
								struct {
									std::uint32_t startOffset;
									std::uint8_t lengthMode;
									std::uint32_t length;
								} full;
								struct {
									std::uint16_t blockId;
									std::uint8_t flags;
								} fast;
							};
						};
						std::uint32_t frequency;
						struct setControl{
							std::uint8_t chipType;
							std::uint8_t command;
							std::uint8_t port;
						};
						struct setData{
							std::uint8_t dataBankId;
							std::uint8_t stepSize;
							std::uint8_t StepBase;
						};
					};
				};
			};

		};

		struct sample_t{
			codec_sample_t left;
			codec_sample_t right;			
		};

		struct wsample_t{
			wcodec_sample_t left;
			wcodec_sample_t right;
		};
	}
}
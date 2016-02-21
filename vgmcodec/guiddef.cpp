#include "stdafx.h"
#include <initguid.h>
#ifdef __cplusplus
extern "C"{
#endif 
	DEFINE_MEDIATYPE_GUID(MFAudioFormat_VGM, FCC('Vgm '));

	// Property GUIDS
	// {B31D90E8-0211-4BF1-8802-6CA7D3BF85F5}
	DEFINE_GUID(MF_MT_VGM_HEADER,
		0xb31d90e8, 0x211, 0x4bf1, 0x88, 0x2, 0x6c, 0xa7, 0xd3, 0xbf, 0x85, 0xf5);

#ifdef __cplusplus
}
#endif
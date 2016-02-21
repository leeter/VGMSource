#ifndef _VGMPLAYSTATE_H
#define _VGMPLAYSTATE_H

#ifndef BEGINENUMCLASS
#ifdef __cplusplus
#define BEGINENUMCLASS(name) enum class name
#else
#define BEGINENUMCLASS(name) typedef enum tag ## name
#endif // __cplusplus
#endif // ENUMCLASS

#ifndef ENDENUMCLASS
#ifdef __cplusplus
#define ENDENUMCLASS(name)	;
#else
#define ENDENUMCLASS(name) name;
#endif
#endif // ENDENUMCLASS

BEGINENUMCLASS(VGM_PLAY_STATE)
{
	STATE_STOPPED = 0x1,
	STATE_PAUSED = 0x2,
	STATE_STARTED = 0x3
}ENDENUMCLASS(VGM_PLAY_STATE)

#ifndef __MIDL_CONST
#ifdef __midl_proxy
#define __MIDL_CONST
#else
#define __MIDL_CONST const
#endif
#endif

#ifndef REF_VGM_PLAY_STATE
#ifdef __cplusplus
#define REF_VGM_PLAY_STATE VGM_PLAY_STATE &
#else
#define REF_VGM_PLAY_STATE VGM_PLAY_STATE * __MIDL_CONST
#endif	// __cplusplus
#endif	//REF_VGM_PLAY_STATE

#endif
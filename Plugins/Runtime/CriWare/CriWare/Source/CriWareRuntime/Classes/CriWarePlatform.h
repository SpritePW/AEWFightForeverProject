/****************************************************************************
*
* CRI Middleware SDK
*
* Copyright (c) 2018 CRI Middleware Co., Ltd.
*
* Library  : CRIWARE plugin for Unreal Engine 4
* Module   : Platform
* File     : CriWarePlatform.h
*
****************************************************************************/
#pragma once

#if !defined(CRIWARE_UE4_LE)	/* <cri_delete_if_LE> */
#endif	/* </cri_delete_if_LE> */
#include "CriWareApi.h"
#include "CriWareMemory.h"

//---------------------------------------------------------
// Platform specific module interfaces
//---------------------------------------------------------

struct FAtomAsrRackConfigsParam
{
	int32 SoundRendererTypeIndex = 0;
	bool bUseOutputPort = false;
	int32 ElementID = 0;
	int32 UserIndex = 0;
};

class ICriWarePlatformAtom {
protected:
	virtual ~ICriWarePlatformAtom() {}
public:
	virtual void InitializeAtomLibrary() {}
	virtual void FinalizeAtomLibrary() {}
	virtual CriAtomExAsrRackId CreateAsrRack(CriAtomExAsrRackConfig* AsrRackConfig, const FAtomAsrRackConfigsParam& AtomAsrRackConfigsParam) { return -1; }
};

#if !defined(CRIWARE_UE4_LE)	/* <cri_delete_if_LE> */
#endif	/* </cri_delete_if_LE> */

class FCriWarePlatform
{
public:
#if !defined(CRIWARE_UE4_LE)	/* <cri_delete_if_LE> */
#else	/* </cri_delete_if_LE> */
	FCriWarePlatform() : IAtom(nullptr) {}
#endif

	FORCEINLINE void RegisterPlatformAtomInterface(ICriWarePlatformAtom* AtomInterface) {
		IAtom = AtomInterface;
	}
	FORCEINLINE void UnregisterPlatformAtomInterface() { IAtom = nullptr; }
	FORCEINLINE ICriWarePlatformAtom* Atom() { return IAtom; }
private:
	ICriWarePlatformAtom* IAtom;

#if !defined(CRIWARE_UE4_LE)	/* <cri_delete_if_LE> */
#endif	/* </cri_delete_if_LE> */
};

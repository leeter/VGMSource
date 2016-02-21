#include "stdafx.h"
#include "lockHelper.h"

using namespace vgmcodec::helpers;

lockHelper::lockHelper(::ATL::CComObjectRootEx<::ATL::CComMultiThreadModel> & comObj)
:comObj(comObj)
{
	comObj.Lock();
}


lockHelper::~lockHelper()
{
	comObj.Unlock();
}

#include "stdafx.h"
#include "bufferLocker.h"

using namespace ::vgmcodec::helpers;

bufferLocker::bufferLocker(IMFMediaBuffer& buffer)
:buffer(buffer),
maxLength(0),
currentLength(0),
pData(nullptr),
hr(E_FAIL)
{
	hr = this->buffer.Lock(
		std::addressof(pData),
		std::addressof(maxLength),
		std::addressof(currentLength));
}


bufferLocker::~bufferLocker()
{
	if (pData){
		this->buffer.Unlock();
	}
}

bufferLocker::operator HRESULT() const
{
	return hr;
}

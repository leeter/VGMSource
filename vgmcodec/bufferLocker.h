#ifndef _BUFFERLOCKER_H
#define _BUFFERLOCKER_H
#pragma once
#include <mfobjects.h>
namespace vgmcodec{
	namespace helpers{
		class bufferLocker
		{
		private:
			HRESULT hr;
			IMFMediaBuffer & buffer;
			bufferLocker(bufferLocker const&) = delete;
			bufferLocker& operator=(bufferLocker const&) = delete;
		public:
			bufferLocker(IMFMediaBuffer& buffer);
			~bufferLocker();
			operator HRESULT() const;

			BYTE * pData;
			DWORD maxLength, currentLength;
		};
	}
}
#endif

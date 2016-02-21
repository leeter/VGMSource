#ifndef __LOCKHELPER_H
#define __LOCKHELPER_H
#pragma once

namespace vgmcodec{
	namespace helpers{
		class lockHelper
		{
		public:
			lockHelper(::ATL::CComObjectRootEx<::ATL::CComMultiThreadModel> &);
			~lockHelper();
		private:
			::ATL::CComObjectRootEx<::ATL::CComMultiThreadModel> &comObj;
		};
	}
}
#endif

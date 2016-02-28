
#include "stdafx.h"
#include "ExceptionTranslator.h"

namespace vgmcodec {
	namespace exceptions {
		[[noreturn]]
		void throw_nullptr()
		{
			throw _com_error(E_POINTER);
		}

		HRESULT ExceptionTranslator::operator()() noexcept
		{
			try
			{
				func();
				return S_OK;
			}
			catch (const _com_error & ex)
			{
				return ex.Error();
			}
			catch (const ATL::CAtlException & ex)
			{
				return static_cast<HRESULT>(ex);
			}
			catch (...)
			{
				return E_FAIL;
			}
		}
	}
}


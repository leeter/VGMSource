
#ifndef EXCEPTION_TRANSLATOR_H
#define EXCEPTION_TRANSLATOR_H

#ifdef _MSC_VER
#pragma once
#endif // _MSC_VER

#include <functional>
#include <utility>
#include <comdef.h>

namespace vgmcodec
{
	namespace exceptions
	{
		struct ExceptionTranslator
		{
			std::function<void()> func;
			HRESULT operator()() noexcept;
		};


		[[noreturn]]
		void throw_nullptr();

		template<typename T>
		void check_pointer(T & a) {
			if (a == nullptr) {
				throw_nullptr();
			}
		}
	}
}

#endif // !EXCEPTION_TRANSLATOR_H


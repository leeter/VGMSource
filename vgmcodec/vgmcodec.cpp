// vgmcodec.cpp : Implementation of DLL Exports.


#include "stdafx.h"
#include "resource.h"
#include "vgmcodec_i.h"
#include "dllmain.h"
#include "xdlldata.h"

#define REG_KEYBASE L"Software\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers\\"
#define FILE_EXT L".vgm"
#define REG_KEYNAME REG_KEYBASE FILE_EXT
// Used to determine whether the DLL can be unloaded by OLE.
STDAPI DllCanUnloadNow(void)
{
#ifdef _MERGE_PROXYSTUB
	HRESULT hr = PrxDllCanUnloadNow();
	if (hr != S_OK)
		return hr;
#endif
	return _AtlModule.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type.
_Check_return_
STDAPI  DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
{
	#ifdef _MERGE_PROXYSTUB
	if (PrxDllGetClassObject(rclsid, riid, ppv) == S_OK)
		return S_OK;
#endif
		return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry.
STDAPI DllRegisterServer(void)
{
	MFT_REGISTER_TYPE_INFO aDecoderInputTypes[] =
	{
		{ MFMediaType_Audio, MFAudioFormat_VGM },
	};

	MFT_REGISTER_TYPE_INFO aDecoderOutputTypes[] =
	{
		{ MFMediaType_Audio, MFAudioFormat_PCM }
	};
	const ::std::wstring ENCODER_NAME(L"Vgm decoder MFT");
	::std::vector<wchar_t> encNameTemp(ENCODER_NAME.size());
	::std::copy(ENCODER_NAME.cbegin(), ENCODER_NAME.cend(), encNameTemp.begin());
	HRESULT hr =
		MFTRegister(
		CLSID_vgmSampleRenderer,
		MFT_CATEGORY_AUDIO_DECODER,
		::std::addressof(encNameTemp[0]),
		0,
		ARRAYSIZE(aDecoderInputTypes),
		aDecoderInputTypes,
		ARRAYSIZE(aDecoderOutputTypes),
		aDecoderOutputTypes,
		nullptr);
	// registers object, typelib and all interfaces in typelib
	if (SUCCEEDED(hr))
		hr = _AtlModule.DllRegisterServer();
	#ifdef _MERGE_PROXYSTUB
	if (FAILED(hr))
		return hr;
	hr = PrxDllRegisterServer();
#endif
	if (SUCCEEDED(hr))
	{
		ATL::CAtlTransactionManager transaction;
		ATL::CRegKey hKey(std::addressof(transaction));
		LSTATUS err = hKey.Create(HKEY_LOCAL_MACHINE, REG_KEYNAME, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE);
		if (err == ERROR_SUCCESS){
			OLECHAR szGUID[64];
			int chars = ::StringFromGUID2(CLSID_vgmByteStreamHandler, szGUID, 64);
			if (chars != 0)
				hKey.SetStringValue(szGUID, L"Video Game Music Source ByteStreamHandler");
		}
		hr = HRESULT_FROM_WIN32(err);
	}
	return hr;
}

// DllUnregisterServer - Removes entries from the system registry.
STDAPI DllUnregisterServer(void)
{
	HRESULT hr = _AtlModule.DllUnregisterServer();
	#ifdef _MERGE_PROXYSTUB
	if (FAILED(hr))
		return hr;
	hr = PrxDllRegisterServer();
	if (FAILED(hr))
		return hr;
	hr = PrxDllUnregisterServer();
#endif
	if (SUCCEEDED(hr)){
		LSTATUS err = ::RegDeleteTreeW(HKEY_LOCAL_MACHINE, REG_KEYNAME);
		hr = HRESULT_FROM_WIN32(err);
	}
	if (SUCCEEDED(hr)){
		hr = MFTUnregister(CLSID_vgmSampleRenderer);
	}
	return hr;
}

// DllInstall - Adds/Removes entries to the system registry per user per machine.
STDAPI DllInstall(BOOL bInstall, _In_opt_ LPCWSTR pszCmdLine)
{
	HRESULT hr = E_FAIL;
	static const wchar_t szUserSwitch[] = L"user";

	if (pszCmdLine != nullptr)
	{
		if (_wcsnicmp(pszCmdLine, szUserSwitch, _countof(szUserSwitch)) == 0)
		{
			ATL::AtlSetPerUserRegistration(true);
		}
	}

	if (bInstall)
	{	
		hr = DllRegisterServer();
		if (FAILED(hr))
		{
			DllUnregisterServer();
		}
	}
	else
	{
		hr = DllUnregisterServer();
	}

	return hr;
}



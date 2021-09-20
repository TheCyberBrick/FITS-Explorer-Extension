#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Propsys.lib")

#include <objbase.h>
#include <shlwapi.h>
#include <thumbcache.h>
#include <shlobj.h>
#include <new>
#include <strsafe.h>

#include "Dll.h"

typedef HRESULT(*PFNCREATEINSTANCE)(REFIID riid, void **ppvObject);
struct CLASS_OBJECT_INIT
{
	const CLSID *pClsid;
	PFNCREATEINSTANCE pfnCreate;
};

extern HRESULT CFITSExplorerExtension_CreateInstance(REFIID riid, void **ppv);
const CLASS_OBJECT_INIT c_rgClassObjectInit[] =
{
	{ &__uuidof(CFITSExplorerExtension), CFITSExplorerExtension_CreateInstance }
};

long g_cRefModule = 0;

// Handle to the DLL's module
HINSTANCE g_hInst = NULL;

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void *)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		g_hInst = hInstance;
		DisableThreadLibraryCalls(hInstance);
	}
	return TRUE;
}

STDAPI DllCanUnloadNow()
{
	// Only allow the DLL to be unloaded after all outstanding references have been released
	return (g_cRefModule == 0) ? S_OK : S_FALSE;
}

void DllAddRef()
{
	InterlockedIncrement(&g_cRefModule);
}

void DllRelease()
{
	InterlockedDecrement(&g_cRefModule);
}

class CClassFactory : public IClassFactory
{
public:
	static HRESULT CreateInstance(REFCLSID clsid, const CLASS_OBJECT_INIT *pClassObjectInits, size_t cClassObjectInits, REFIID riid, void **ppv)
	{
		*ppv = NULL;
		HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;
		for (size_t i = 0; i < cClassObjectInits; i++)
		{
			if (clsid == *pClassObjectInits[i].pClsid)
			{
				IClassFactory *pClassFactory = new (std::nothrow) CClassFactory(pClassObjectInits[i].pfnCreate);
				hr = pClassFactory ? S_OK : E_OUTOFMEMORY;
				if (SUCCEEDED(hr))
				{
					hr = pClassFactory->QueryInterface(riid, ppv);
					pClassFactory->Release();
				}
				break; // match found
			}
		}
		return hr;
	}

	CClassFactory(PFNCREATEINSTANCE pfnCreate) : _cRef(1), _pfnCreate(pfnCreate)
	{
		DllAddRef();
	}

	IFACEMETHODIMP QueryInterface(REFIID riid, void ** ppv)
	{
		static const QITAB qit[] =
		{
			QITABENT(CClassFactory, IClassFactory),
			{ 0 }
		};
		return QISearch(this, qit, riid, ppv);
	}

	IFACEMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&_cRef);
	}

	IFACEMETHODIMP_(ULONG) Release()
	{
		long cRef = InterlockedDecrement(&_cRef);
		if (cRef == 0)
		{
			delete this;
		}
		return cRef;
	}

	IFACEMETHODIMP CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
	{
		return punkOuter ? CLASS_E_NOAGGREGATION : _pfnCreate(riid, ppv);
	}

	IFACEMETHODIMP LockServer(BOOL fLock)
	{
		if (fLock)
		{
			DllAddRef();
		}
		else
		{
			DllRelease();
		}
		return S_OK;
	}

private:
	~CClassFactory()
	{
		DllRelease();
	}

	long _cRef;
	PFNCREATEINSTANCE _pfnCreate;
};

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv)
{
	return CClassFactory::CreateInstance(clsid, c_rgClassObjectInit, ARRAYSIZE(c_rgClassObjectInit), riid, ppv);
}

struct REGISTRY_ENTRY
{
	HKEY   hkeyRoot;
	PCWSTR pszKeyName;
	PCWSTR pszValueName;
	PCWSTR pszData;
};

HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY *pRegistryEntry)
{
	HKEY hKey;
	HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyExW(pRegistryEntry->hkeyRoot, pRegistryEntry->pszKeyName,
		0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL));
	if (SUCCEEDED(hr))
	{
		hr = HRESULT_FROM_WIN32(RegSetValueExW(hKey, pRegistryEntry->pszValueName, 0, REG_SZ,
			(LPBYTE)pRegistryEntry->pszData,
			((DWORD)wcslen(pRegistryEntry->pszData) + 1) * sizeof(WCHAR)));
		RegCloseKey(hKey);
	}
	return hr;
}

HRESULT CreateRegKeyAndSetValuef(HKEY hkey, PCWSTR pszValueName, PCWSTR pszData, PCWSTR pszKeyNameFormat, ...)
{
	WCHAR szKeyName[256];

	va_list args;
	va_start(args, pszKeyNameFormat);

	HRESULT hr = StringCchVPrintf(szKeyName, ARRAYSIZE(szKeyName), pszKeyNameFormat, args);
	if (SUCCEEDED(hr))
	{
		REGISTRY_ENTRY entry = { hkey, szKeyName, pszValueName, pszData };
		hr = CreateRegKeyAndSetValue(&entry);
	}

	va_end(args);

	return hr;
}

HRESULT DeleteRegKeyf(HKEY hkey, PCWSTR pszKeyNameFormat, ...)
{
	WCHAR szKeyName[256];

	va_list args;
	va_start(args, pszKeyNameFormat);

	HRESULT hr = StringCchVPrintf(szKeyName, ARRAYSIZE(szKeyName), pszKeyNameFormat, args);
	if (SUCCEEDED(hr))
	{
		hr = HRESULT_FROM_WIN32(RegDeleteTreeW(hkey, szKeyName));
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
		{
			// If the registry entry has already been deleted, say S_OK.
			hr = S_OK;
		}
	}

	va_end(args);

	return hr;
}

STDAPI DllRegisterServer()
{
	HRESULT hr;

	WCHAR szModuleName[MAX_PATH];

	if (!GetModuleFileNameW(g_hInst, szModuleName, ARRAYSIZE(szModuleName)))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
	}
	else
	{
		WCHAR szCLSID[39];
		StringFromGUID2(__uuidof(CFITSExplorerExtension), szCLSID, ARRAYSIZE(szCLSID));

		// Register COM server
		hr = CreateRegKeyAndSetValuef(HKEY_CURRENT_USER, NULL, SZ_PROG_ID, L"Software\\Classes\\CLSID\\%s", szCLSID);
		if (FAILED(hr))
		{
			hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, NULL, SZ_PROG_ID, L"Software\\Classes\\CLSID\\%s", szCLSID);
			if (FAILED(hr))
			{
				return hr;
			}
		}

		hr = CreateRegKeyAndSetValuef(HKEY_CURRENT_USER, NULL, szModuleName, L"Software\\Classes\\CLSID\\%s\\InProcServer32", szCLSID);
		if (FAILED(hr))
		{
			hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, NULL, szModuleName, L"Software\\Classes\\CLSID\\%s\\InProcServer32", szCLSID);
			if (FAILED(hr))
			{
				return hr;
			}
		}

		hr = CreateRegKeyAndSetValuef(HKEY_CURRENT_USER, L"ThreadingModel", L"Apartment", L"Software\\Classes\\CLSID\\%s\\InProcServer32", szCLSID);
		if (FAILED(hr))
		{
			hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, L"ThreadingModel", L"Apartment", L"Software\\Classes\\CLSID\\%s\\InProcServer32", szCLSID);
			if (FAILED(hr))
			{
				return hr;
			}
		}

		// Register thumbnail shell extension
		for (int i = 0; i < ARRAYSIZE(SZ_FILE_EXT); i++)
		{
			hr = CreateRegKeyAndSetValuef(HKEY_CURRENT_USER, NULL, szCLSID, L"Software\\Classes\\.%s\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", SZ_FILE_EXT[i]);
			if (FAILED(hr))
			{
				hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, NULL, szCLSID, L"Software\\Classes\\.%s\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", SZ_FILE_EXT[i]);
				if (FAILED(hr))
				{
					return hr;
				}
			}
		}

		// Register property handler
		for (int i = 0; i < ARRAYSIZE(SZ_FILE_EXT); i++)
		{
			hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, NULL, szCLSID, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.%s", SZ_FILE_EXT[i]);
			if (FAILED(hr))
			{
				return hr;
			}
		}

		// Register ProgID
		hr = CreateRegKeyAndSetValuef(HKEY_CURRENT_USER, NULL, SZ_PROG_NAME, L"Software\\Classes\\%s", SZ_PROG_ID);
		if (FAILED(hr))
		{
			hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, NULL, SZ_PROG_NAME, L"Software\\Classes\\%s", SZ_PROG_ID);
			if (FAILED(hr))
			{
				return hr;
			}
		}
		
		hr = CreateRegKeyAndSetValuef(HKEY_CURRENT_USER, L"FriendlyTypeName", SZ_PROG_NAME, L"Software\\Classes\\%s", SZ_PROG_ID);
		if (FAILED(hr))
		{
			hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, L"FriendlyTypeName", SZ_PROG_NAME, L"Software\\Classes\\%s", SZ_PROG_ID);
			if (FAILED(hr))
			{
				return hr;
			}
		}

		hr = CreateRegKeyAndSetValuef(HKEY_CURRENT_USER, NULL, szCLSID, L"Software\\Classes\\%s\\CLSID", SZ_PROG_ID);
		if (FAILED(hr))
		{
			hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, NULL, szCLSID, L"Software\\Classes\\%s\\CLSID", SZ_PROG_ID);
			if (FAILED(hr))
			{
				return hr;
			}
		}

		// Register property lists in ProgID
		for (int i = 0; i < ARRAYSIZE(PROPERTY_LISTS); i++)
		{
			PROPERTYLIST prop = PROPERTY_LISTS[i];
			hr = CreateRegKeyAndSetValuef(HKEY_CURRENT_USER, prop.pszName, prop.pszValue, L"Software\\Classes\\%s", SZ_PROG_ID);
			if (FAILED(hr))
			{
				hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, prop.pszName, prop.pszValue, L"Software\\Classes\\%s", SZ_PROG_ID);
				if (FAILED(hr))
				{
					return hr;
				}
			}
		}

		// Register ProgID to extensions
		for (int i = 0; i < ARRAYSIZE(SZ_FILE_EXT); i++)
		{
			hr = CreateRegKeyAndSetValuef(HKEY_CURRENT_USER, NULL, SZ_PROG_ID, L"Software\\Classes\\.%s", SZ_FILE_EXT[i]);
			if (FAILED(hr))
			{
				hr = CreateRegKeyAndSetValuef(HKEY_LOCAL_MACHINE, NULL, SZ_PROG_ID, L"Software\\Classes\\.%s", SZ_FILE_EXT[i]);
				if (FAILED(hr))
				{
					return hr;
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
	}

	return hr;
}

STDAPI DllUnregisterServer()
{
	HRESULT hr = S_OK;

	WCHAR szCLSID[39];
	StringFromGUID2(__uuidof(CFITSExplorerExtension), szCLSID, ARRAYSIZE(szCLSID));

	// Unregister COM server
	hr = DeleteRegKeyf(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\%s", szCLSID);
	if (FAILED(hr))
	{
		return hr;
	}

	// Unregister thumbnail shell extension
	for (int i = 0; i < ARRAYSIZE(SZ_FILE_EXT); i++)
	{
		hr = DeleteRegKeyf(HKEY_CURRENT_USER, L"Software\\Classes\\.%s\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", SZ_FILE_EXT[i]);
		if (FAILED(hr))
		{
			return hr;
		}

		hr = DeleteRegKeyf(HKEY_LOCAL_MACHINE, L"Software\\Classes\\.%s\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", SZ_FILE_EXT[i]);
		if (FAILED(hr))
		{
			return hr;
		}
	}

	// Unregister property handler
	for (int i = 0; i < ARRAYSIZE(SZ_FILE_EXT); i++)
	{
		hr = DeleteRegKeyf(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.%s", SZ_FILE_EXT[i]);
		if (FAILED(hr))
		{
			return hr;
		}
	}

	// Unegister ProgID
	hr = DeleteRegKeyf(HKEY_LOCAL_MACHINE, L"Software\\Classes\\%s", SZ_PROG_ID);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = DeleteRegKeyf(HKEY_CURRENT_USER, L"Software\\Classes\\%s", SZ_PROG_ID);
	if (FAILED(hr))
	{
		return hr;
	}

	return hr;
}

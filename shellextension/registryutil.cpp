#include "registryutil.h"
#include "constants.h"

HRESULT CreateRegKeyAndSetValue(const RegistryEntry *pRegistryEntry)
{
	HKEY hKey;
	HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyEx(pRegistryEntry->hkeyRoot, pRegistryEntry->pszKeyName,
		0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL));
	if (SUCCEEDED(hr))
	{
		hr = HRESULT_FROM_WIN32(RegSetValueEx(hKey, pRegistryEntry->pszValueName, 0, REG_SZ,
			(LPBYTE)pRegistryEntry->pszData,
			((DWORD)wcslen(pRegistryEntry->pszData) + 1) * sizeof(WCHAR)));
		RegCloseKey(hKey);
	}
	return hr;
}

HRESULT CreateRegKeyAndSetValuef(HKEY hkey, PCWSTR pszValueName, PCWSTR pszData, PCWSTR pszKeyNameFormat, ...)
{
	WCHAR szKeyName[256];
	szKeyName[0] = '\0';

	va_list args;
	va_start(args, pszKeyNameFormat);

	HRESULT hr = StringCchVPrintf(szKeyName, ARRAYSIZE(szKeyName), pszKeyNameFormat, args);

	va_end(args);

	if (SUCCEEDED(hr))
	{
		RegistryEntry entry = { hkey, szKeyName, pszValueName, pszData };
		hr = CreateRegKeyAndSetValue(&entry);
	}

	return hr;
}

HRESULT DeleteRegKeyf(HKEY hkey, PCWSTR pszKeyNameFormat, ...)
{
	WCHAR szKeyName[256];

	va_list args;
	va_start(args, pszKeyNameFormat);

	HRESULT hr = StringCchVPrintf(szKeyName, ARRAYSIZE(szKeyName), pszKeyNameFormat, args);

	va_end(args);

	if (SUCCEEDED(hr))
	{
		hr = HRESULT_FROM_WIN32(RegDeleteTree(hkey, szKeyName));
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
		{
			// If the registry entry has already been deleted, say S_OK.
			hr = S_OK;
		}
	}

	return hr;
}

HRESULT DeleteRegKeyIfSetf(HKEY hkeyRoot, PCWSTR pszVal, PCWSTR pszKeyNameFormat, ...)
{
	WCHAR szKeyName[256];

	va_list args;
	va_start(args, pszKeyNameFormat);

	HRESULT hr = StringCchVPrintf(szKeyName, ARRAYSIZE(szKeyName), pszKeyNameFormat, args);

	va_end(args);

	if (SUCCEEDED(hr))
	{
		HKEY hKey;
		hr = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_READ, &hKey));
		if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && hr != HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
		{
			RegCloseKey(hKey);
		}
		else if (SUCCEEDED(hr))
		{
			const DWORD cBufferSize = 128;
			WCHAR szBuffer[cBufferSize];
			szBuffer[0] = '\0';
			DWORD dwBufferSize = cBufferSize;

			DWORD type;
			hr = HRESULT_FROM_WIN32(RegQueryValueEx(hKey, NULL, NULL, &type, (LPBYTE)szBuffer, &dwBufferSize));

			RegCloseKey(hKey);

			if (SUCCEEDED(hr))
			{
				// Check that the value is currently set to the specified value before deleting it
				if (dwBufferSize <= 0 /*no value*/ || (type == REG_SZ && lstrcmp(szBuffer, pszVal)))
				{
					hr = DeleteRegKeyf(HKEY_CURRENT_USER, szKeyName);
				}
			}
		}
		else
		{
			// Key was already deleted at some point
			RegCloseKey(hKey);
			hr = S_OK;
		}
	}

	return hr;
}

HRESULT CreateSettingsRegKeyAndSetValue(HKEY hkeyRoot, PCWSTR pszValueName, PCWSTR pszData)
{
	WCHAR szKeyName[256];

	HRESULT hr = StringCchPrintf(szKeyName, ARRAYSIZE(szKeyName), L"Software\\%s", SZ_PROG_ID);

	if (SUCCEEDED(hr))
	{
		HKEY hKey;
		hr = HRESULT_FROM_WIN32(RegCreateKeyEx(hkeyRoot, szKeyName,
			0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL));
		if (SUCCEEDED(hr))
		{
			hr = HRESULT_FROM_WIN32(RegSetValueEx(hKey, pszValueName, 0, REG_SZ,
				(LPBYTE)pszData,
				((DWORD)wcslen(pszData) + 1) * sizeof(WCHAR)));
			RegCloseKey(hKey);
		}
	}
	
	return hr;
}

HRESULT CreateSettingsRegKeyAndSetValue(HKEY hkeyRoot, PCWSTR pszValueName, DWORD pszData)
{
	WCHAR szKeyName[256];

	HRESULT hr = StringCchPrintf(szKeyName, ARRAYSIZE(szKeyName), L"Software\\%s", SZ_PROG_ID);

	if (SUCCEEDED(hr))
	{
		HKEY hKey;
		hr = HRESULT_FROM_WIN32(RegCreateKeyEx(hkeyRoot, szKeyName,
			0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL));
		if (SUCCEEDED(hr))
		{
			hr = HRESULT_FROM_WIN32(RegSetValueEx(hKey, pszValueName, 0, REG_DWORD,
				(LPBYTE)&pszData,
				sizeof(DWORD)));
			RegCloseKey(hKey);
		}
	}

	return hr;
}

HRESULT GetSettingsRegKeyValue(HKEY hkeyRoot, PCWSTR pszValueName, LPWSTR pszData, LPDWORD lpcbData)
{
	if (*lpcbData <= 0)
	{
		return E_FAIL;
	}

	WCHAR szKeyName[256];

	HRESULT hr = StringCchPrintf(szKeyName, ARRAYSIZE(szKeyName), L"Software\\%s", SZ_PROG_ID);

	if (SUCCEEDED(hr))
	{
		HKEY hKey;
		hr = HRESULT_FROM_WIN32(RegOpenKeyEx(hkeyRoot, szKeyName, 0, KEY_READ, &hKey));
		if (FAILED(hr))
		{
			RegCloseKey(hKey);
			return hr;
		}

		pszData[0] = '\0';

		DWORD type;
		hr = HRESULT_FROM_WIN32(RegQueryValueEx(hKey, pszValueName, NULL, &type, (LPBYTE)pszData, lpcbData));
		
		RegCloseKey(hKey);
		
		if (FAILED(hr))
		{
			return hr;
		}

		if (type != REG_SZ || *lpcbData <= 0)
		{
			hr = E_FAIL;
			return hr;
		}
	}

	return hr;
}

HRESULT GetSettingsRegKeyValue(HKEY hkeyRoot, PCWSTR pszValueName, LPDWORD pszData)
{
	WCHAR szKeyName[256];

	HRESULT hr = StringCchPrintf(szKeyName, ARRAYSIZE(szKeyName), L"Software\\%s", SZ_PROG_ID);

	if (SUCCEEDED(hr))
	{
		HKEY hKey;
		hr = HRESULT_FROM_WIN32(RegOpenKeyEx(hkeyRoot, szKeyName, 0, KEY_READ, &hKey));
		if (FAILED(hr))
		{
			RegCloseKey(hKey);
			return hr;
		}

		DWORD size = sizeof(DWORD);

		DWORD type;
		hr = HRESULT_FROM_WIN32(RegQueryValueEx(hKey, pszValueName, NULL, &type, (LPBYTE)pszData, &size));

		RegCloseKey(hKey);

		if (FAILED(hr))
		{
			return hr;
		}

		if (type != REG_DWORD || size <= 0)
		{
			hr = E_FAIL;
			return hr;
		}
	}

	return hr;
}
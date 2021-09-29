#pragma once

#include <shlwapi.h>
#include <winreg.h>
#include <strsafe.h>

struct RegistryEntry
{
	HKEY   hkeyRoot;
	PCWSTR pszKeyName;
	PCWSTR pszValueName;
	PCWSTR pszData;
};

HRESULT CreateRegKeyAndSetValue(const RegistryEntry *pRegistryEntry);
HRESULT CreateRegKeyAndSetValuef(HKEY hkey, PCWSTR pszValueName, PCWSTR pszData, PCWSTR pszKeyNameFormat, ...);

HRESULT DeleteRegKeyf(HKEY hkey, PCWSTR pszKeyNameFormat, ...);
HRESULT DeleteRegKeyIfSetf(HKEY hkey, PCWSTR pszVal, PCWSTR pszKeyNameFormat, ...);

HRESULT CreateSettingsRegKeyAndSetValue(HKEY hkey, PCWSTR pszValueName, PCWSTR pszData);
HRESULT CreateSettingsRegKeyAndSetValue(HKEY hkey, PCWSTR pszValueName, DWORD pszData);

HRESULT GetSettingsRegKeyValue(HKEY hkey, PCWSTR pszValueName, LPWSTR pszData, LPDWORD lpcbData);
HRESULT GetSettingsRegKeyValue(HKEY hkey, PCWSTR pszValueName, LPDWORD pszData);
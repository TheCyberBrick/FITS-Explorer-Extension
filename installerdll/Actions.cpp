#include "stdafx.h"

HRESULT HRESULTFromRegAction(ULONG status)
{
	if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
	{
		return S_FALSE;
	}
	else if (status == ERROR_SUCCESS)
	{
		return S_OK;
	}
	return HRESULT_FROM_WIN32(status);
}

struct AssocApp
{
	AssocApp(HKEY hkey, std::wstring regKey, std::wstring appName, std::wstring fileExt)
		: data(std::make_tuple(hkey, regKey, appName, fileExt))
	{
	}

	HKEY const hKey() const { return std::get<0>(data); };
	std::wstring const& regKey() const { return std::get<1>(data); };
	std::wstring const& appName() const { return std::get<2>(data); };
	std::wstring const& fileExt() const { return std::get<3>(data); };

	bool operator<(AssocApp const& other) const
	{
		return data < other.data;
	}

	bool operator==(AssocApp const& other) const
	{
		return data == other.data;
	}
private:
	std::tuple<HKEY, std::wstring, std::wstring, std::wstring> data;
};

std::wstring GetDisplayAppName(std::wstring& appName)
{
	std::wstring displayAppName(appName);
	if (displayAppName.length() > 4)
	{
		std::wstring suffix = displayAppName.substr(displayAppName.length() - 4);
		if (suffix == L".exe")
		{
			displayAppName = displayAppName.substr(0, displayAppName.length() - 4);
		}
	}
	return displayAppName;
}

HRESULT FindExistingFileClasses(HKEY hKeyRoot, PCWSTR fileExt, std::set<AssocApp> &apps)
{
	WCHAR szFileClassKeyName[128];
	szFileClassKeyName[0] = '\0';

	HRESULT hr = StringCchPrintf(szFileClassKeyName, ARRAYSIZE(szFileClassKeyName), L"Software\\Classes\\.%s", fileExt);
	if (FAILED(hr))
	{
		return hr;
	}

	HKEY hKey;
	hr = HRESULTFromRegAction(RegOpenKeyEx(hKeyRoot, szFileClassKeyName, 0, KEY_READ, &hKey));
	if (FAILED(hr))
	{
		RegCloseKey(hKey);
		return hr;
	}

	if (hr == S_OK)
	{
		const DWORD cBufferSize = 128;
		WCHAR szBuffer[cBufferSize];
		szBuffer[0] = '\0';
		DWORD dwBufferSize = cBufferSize;

		DWORD type;
		hr = HRESULTFromRegAction(RegQueryValueEx(hKey, NULL, NULL, &type, (LPBYTE)szBuffer, &dwBufferSize));
		
		RegCloseKey(hKey);

		if (FAILED(hr))
		{
			return hr;
		}

		if (hr == S_OK && type == REG_SZ && dwBufferSize > 0)
		{
			WCHAR szClassGroupKeyName[256];
			szClassGroupKeyName[0] = '\0';

			hr = StringCchPrintf(szClassGroupKeyName, ARRAYSIZE(szClassGroupKeyName), L"Software\\Classes\\%s", szBuffer);
			if (FAILED(hr))
			{
				return hr;
			}

			hr = HRESULTFromRegAction(RegOpenKeyEx(hKeyRoot, szClassGroupKeyName, 0, KEY_READ, &hKey));
			if (FAILED(hr))
			{
				return hr;
			}

			RegCloseKey(hKey);

			if (hr == S_OK)
			{
				std::wstring appName(szBuffer);

				// Try using the value of shell\open (i.e. the
				// application the file will be opened with)
				// as the app name. If it doesn't exist it falls
				// back to the class group name.

				WCHAR szShellOpenKeyName[256];
				szShellOpenKeyName[0] = '\0';

				hr = StringCchPrintf(szShellOpenKeyName, ARRAYSIZE(szShellOpenKeyName), L"Software\\Classes\\%s\\shell\\open", szBuffer);
				if (FAILED(hr))
				{
					return hr;
				}

				hr = HRESULTFromRegAction(RegOpenKeyEx(hKeyRoot, szShellOpenKeyName, 0, KEY_READ, &hKey));
				if (FAILED(hr))
				{
					return hr;
				}

				if (hr == S_OK)
				{
					szBuffer[0] = '\0';
					dwBufferSize = cBufferSize;

					hr = HRESULTFromRegAction(RegQueryValueEx(hKey, NULL, NULL, &type, (LPBYTE)szBuffer, &dwBufferSize));

					RegCloseKey(hKey);

					if (FAILED(hr))
					{
						return hr;
					}

					if (hr == S_OK && type == REG_SZ && dwBufferSize > 0)
					{
						appName = std::wstring(szBuffer);
					}
				}
				else
				{
					RegCloseKey(hKey);
				}

				// Found existing class group that already
				// claimed file type
				AssocApp app(hKeyRoot, std::wstring(szClassGroupKeyName), GetDisplayAppName(appName), std::wstring(fileExt));
				apps.emplace(app);
			}
		}
	}
	else
	{
		RegCloseKey(hKey);
	}

	return hr;
}

HRESULT FindAssociatedApplications(HKEY hKeyRoot, PCWSTR fileExt, std::set<AssocApp> &apps)
{
	WCHAR szOpenWithListKeyName[256];
	szOpenWithListKeyName[0] = '\0';

	HRESULT hr = StringCchPrintf(szOpenWithListKeyName, ARRAYSIZE(szOpenWithListKeyName), L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.%s\\OpenWithList", fileExt);
	if (FAILED(hr))
	{
		return hr;
	}

	HKEY hKey;
	hr = HRESULTFromRegAction(RegOpenKeyEx(hKeyRoot, szOpenWithListKeyName, 0, KEY_READ, &hKey));
	if (FAILED(hr))
	{
		RegCloseKey(hKey);
		return hr;
	}

	if (hr == S_OK)
	{
		const DWORD cBufferSize = 128;
		WCHAR szBuffer[cBufferSize];
		szBuffer[0] = '\0';
		DWORD dwBufferSize = cBufferSize;

		DWORD type;
		hr = HRESULTFromRegAction(RegQueryValueEx(hKey, L"MRUList", NULL, &type, (LPBYTE)szBuffer, &dwBufferSize));
		if (FAILED(hr))
		{
			RegCloseKey(hKey);
			return hr;
		}

		if (hr == S_OK && type == REG_SZ && dwBufferSize > 0)
		{
			// MRUList value exists, get currently associated application
			WCHAR szAssocValueName[] = { szBuffer[0], '\0' };

			dwBufferSize = cBufferSize;
			hr = HRESULTFromRegAction(RegQueryValueEx(hKey, szAssocValueName, NULL, &type, (LPBYTE)szBuffer, &dwBufferSize));

			RegCloseKey(hKey);

			if (FAILED(hr))
			{
				return hr;
			}

			if (hr == S_OK && type == REG_SZ && dwBufferSize > 0)
			{
				WCHAR szAssocAppKeyName[256];
				szAssocAppKeyName[0] = '\0';

				hr = StringCchPrintf(szAssocAppKeyName, ARRAYSIZE(szAssocAppKeyName), L"Software\\Classes\\Applications\\%s", szBuffer);
				if (FAILED(hr))
				{
					return hr;
				}

				hr = HRESULTFromRegAction(RegOpenKeyEx(hKeyRoot, szAssocAppKeyName, 0, KEY_READ, &hKey));
				if (FAILED(hr))
				{
					return hr;
				}

				RegCloseKey(hKey);

				if (hr == S_OK)
				{
					std::wstring appName(szBuffer);

					// Found associated application
					AssocApp app(hKeyRoot, std::wstring(L"Software\\Classes\\Applications\\") + appName, GetDisplayAppName(appName), std::wstring(fileExt));
					apps.emplace(app);
				}
			}
		}
		else
		{
			// No associated application, don't need to do anything
			RegCloseKey(hKey);
		}
	}
	else
	{
		RegCloseKey(hKey);
	}

	return hr;
}

UINT __stdcall SetAssociatedApplications(
	MSIHANDLE hInstall
)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	std::set<AssocApp> apps;

	hr = WcaInitialize(hInstall, "SetAssociatedApplications");
	ExitOnFailure(hr, "Failed to initialize");

	WcaLog(LOGMSG_STANDARD, "Initialized");

	for (int i = 0; i < ARRAYSIZE(SZ_FILE_EXT); i++)
	{
		std::wstring wext = std::wstring(SZ_FILE_EXT[i]);
		std::string log(wext.begin(), wext.end());
		log = "Searching associated applications for ." + log;
		WcaLog(LOGMSG_STANDARD, log.c_str());

		hr = FindAssociatedApplications(HKEY_CURRENT_USER, SZ_FILE_EXT[i], apps);
		ExitOnFailure(hr, "Failed to find associated applications in HKCU");

		hr = FindAssociatedApplications(HKEY_LOCAL_MACHINE, SZ_FILE_EXT[i], apps);
		ExitOnFailure(hr, "Failed to find associated applications in HKLM");
		
		hr = FindExistingFileClasses(HKEY_CURRENT_USER, SZ_FILE_EXT[i], apps);
		ExitOnFailure(hr, "Failed to find existing file classes in HKCU");

		hr = FindExistingFileClasses(HKEY_LOCAL_MACHINE, SZ_FILE_EXT[i], apps);
		ExitOnFailure(hr, "Failed to find existing file classes in HKLM");
	}

	int nhkcu = 0;
	int nhklm = 0;
	for (auto const& app : apps)
	{
		HKEY const hKey = app.hKey();
		std::wstring const regKey = app.regKey();
		std::wstring const appName = app.appName();
		std::wstring const appExt = app.fileExt();

		std::string log(appName.begin(), appName.end());
		log = "Found associated application: " + log;
		WcaLog(LOGMSG_STANDARD, log.c_str());

		int assocAppIndex;
		std::wstring hkeyWStr;
		if (hKey == HKEY_CURRENT_USER)
		{
			hkeyWStr = L"HKCU";
			assocAppIndex = nhkcu;
		}
		else if (hKey == HKEY_LOCAL_MACHINE)
		{
			hkeyWStr = L"HKLM";
			assocAppIndex = nhklm;
		}
		else
		{
			hr = E_INVALIDSTATE;
			ExitOnFailure(hr, "Invalid hKey");
		}

		for (int propertyIndex = 0; propertyIndex < ARRAYSIZE(PROPERTY_LISTS); propertyIndex++)
		{
			PROPERTYLIST prop = PROPERTY_LISTS[propertyIndex];

			std::wstringstream regRootWProp;
			regRootWProp << L"AssocApp_" << hkeyWStr << "_" << assocAppIndex << L"_Root_" << propertyIndex;
			hr = WcaSetProperty(regRootWProp.str().c_str(), hkeyWStr.c_str());
			ExitOnFailure(hr, "Failed to set property");

			std::wstringstream regKeyWProp;
			regKeyWProp << L"AssocApp_" << hkeyWStr << "_" << assocAppIndex << L"_Key_" << propertyIndex;
			hr = WcaSetProperty(regKeyWProp.str().c_str(), regKey.c_str());
			ExitOnFailure(hr, "Failed to set property");

			std::wstringstream regNameWProp;
			regNameWProp << L"AssocApp_" << hkeyWStr << "_" << assocAppIndex << L"_Name_" << propertyIndex;
			hr = WcaSetProperty(regNameWProp.str().c_str(), prop.pszName);
			ExitOnFailure(hr, "Failed to set property");

			std::wstringstream regValueWProp;
			regValueWProp << L"AssocApp_" << hkeyWStr << "_" << assocAppIndex << L"_Value_" << propertyIndex;
			hr = WcaSetProperty(regValueWProp.str().c_str(), prop.pszValue);
			ExitOnFailure(hr, "Failed to set property");

			std::wstringstream regTypeWProp;
			regTypeWProp << L"AssocApp_" << hkeyWStr << "_" << assocAppIndex << L"_Type_" << propertyIndex;
			hr = WcaSetProperty(regTypeWProp.str().c_str(), L"string");
			ExitOnFailure(hr, "Failed to set property");
		}

		std::wstringstream assocAppPropW;
		assocAppPropW << L"AssocApp_" << hkeyWStr << "_" << assocAppIndex;
		hr = WcaSetProperty(assocAppPropW.str().c_str(), appName.c_str());
		ExitOnFailure(hr, "Failed to set property");

		PMSIHANDLE hFeatureRec = MsiCreateRecord(1);
		if (!hFeatureRec)
		{
			hr = E_OUTOFMEMORY;
			ExitOnFailure(hr, "Failed to create record for Feature record lookup");
		}

		PMSIHANDLE hFeatureTableView;
		hr = WcaOpenExecuteView(L"SELECT * FROM `Feature`", &hFeatureTableView);
		ExitOnFailure(hr, "Failed to open SELECT query view on Feature table");

		while ((hr = WcaFetchRecord(hFeatureTableView, &hFeatureRec)) == S_OK)
		{
			LPWSTR szKey = NULL;
			hr = WcaGetRecordString(hFeatureRec, 1 /*Feature*/, &szKey);
			ExitOnFailure(hr, "Failed to get key of Feature record");
			std::wstring key = std::wstring(szKey);
			ReleaseStr(szKey);

			LPWSTR szTitle = NULL;
			hr = WcaGetRecordString(hFeatureRec, 3 /*Title*/, &szTitle);
			ExitOnFailure(hr, "Failed to get Title of Feature record");
			std::wstring title = std::wstring(szTitle);
			ReleaseStr(szTitle);

			std::wstringstream assocAppPropKeyW;
			assocAppPropKeyW << "[" << assocAppPropW.str() << "]";
			std::wstring assocAppPropKeyWStr(assocAppPropKeyW.str());

			std::wstringstream assocExtPropKeyW;
			assocExtPropKeyW << "[" << assocAppPropW.str() << "_EXT]";
			std::wstring assocExtPropKeyWStr(assocExtPropKeyW.str());

			if (title.find(assocAppPropKeyWStr) != std::wstring::npos || title.find(assocExtPropKeyWStr) != std::wstring::npos)
			{
				size_t index = 0;
				while ((index = title.find(assocAppPropKeyWStr, index)) != std::wstring::npos) {
					title.replace(index, assocAppPropKeyWStr.length(), appName);
					index += appName.length();
				}

				index = 0;
				while ((index = title.find(assocExtPropKeyWStr, index)) != std::wstring::npos) {
					title.replace(index, assocExtPropKeyWStr.length(), appExt);
					index += appExt.length();
				}

				hr = WcaSetRecordString(hFeatureRec, 3 /*Title*/, title.c_str());
				ExitOnFailure(hr, "Failed to set Title of Feature record");

				PMSIHANDLE hFeatureRecordView;

				std::wstringstream delQuery;
				delQuery << L"DELETE FROM `Feature` WHERE `Feature`.`Feature` = '" << key << "'";
				hr = WcaOpenExecuteView(delQuery.str().c_str(), &hFeatureRecordView);
				ExitOnFailure(hr, "Failed to delete Feature record");

				hr = WcaOpenView(L"INSERT INTO `Feature` (`Feature`, `Feature_Parent`, `Title`, `Description`, `Display`, `Level`, `Directory_`, `Attributes`) VALUES (?, ?, ?, ?, ?, ?, ?, ?) TEMPORARY", &hFeatureRecordView);
				ExitOnFailure(hr, "Failed to open INSERT query view on Feature table");

				hr = WcaExecuteView(hFeatureRecordView, hFeatureRec);
				ExitOnFailure(hr, "Failed to execute INSERT query on Feature table");

				std::stringstream log;
				std::string keySStr(key.begin(), key.end());
				std::string featureTitleSStr(title.begin(), title.end());
				log << "Set title of feature '" << keySStr << "' to '" << featureTitleSStr << "'";
				WcaLog(LOGMSG_STANDARD, log.str().c_str());
			}
		}
		if (hr == E_NOMOREITEMS) {
			// End of table is no error
			hr = S_OK;
		}
		ExitOnFailure(hr, "Failed to get Feature record");

		if (hKey == HKEY_CURRENT_USER)
		{
			nhkcu++;
		}
		else if (hKey == HKEY_LOCAL_MACHINE)
		{
			nhklm++;
		}
	}

LExit:
	er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	return WcaFinalize(er);
}

UINT __stdcall NotifyShellAssocChanged(
	MSIHANDLE hInstall
)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	hr = WcaInitialize(hInstall, "NotifyShellAssocChanged");
	ExitOnFailure(hr, "Failed to initialize");

	WcaLog(LOGMSG_STANDARD, "Initialized");

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	WcaLog(LOGMSG_STANDARD, "Notified shell of assoc change");

LExit:
	er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	return WcaFinalize(er);
}

// DllMain - Initialize and cleanup WiX custom action utils.
extern "C" BOOL WINAPI DllMain(
	__in HINSTANCE hInst,
	__in ULONG ulReason,
	__in LPVOID
)
{
	switch (ulReason)
	{
	case DLL_PROCESS_ATTACH:
		WcaGlobalInitialize(hInst);
		break;

	case DLL_PROCESS_DETACH:
		WcaGlobalFinalize();
		break;
	}

	return TRUE;
}

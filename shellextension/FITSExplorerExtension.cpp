#include <shlwapi.h>
#include <thumbcache.h>
#include <propkey.h>
#include <propvarutil.h>
#include <strsafe.h>

#include <sstream>
#include <valarray>

//Debug
#include <WinUser.h>

#include "Dll.h"
#include "fitsloader.h"
#include "registryutil.h"

void DllAddRef();
void DllRelease();

class CFITSExplorerExtension :
	public IInitializeWithStream,
	public IThumbnailProvider,
	public IPropertyStore,
	public IPropertyStoreCapabilities
{
public:
	CFITSExplorerExtension() : _cRef(1), _pSettings(NULL), _pStream(NULL), _pFITSPropertyCache(NULL), _pFITSInfo(NULL)
	{
		DllAddRef();
	}

	virtual ~CFITSExplorerExtension()
	{
		if (_pStream)
		{
			_pStream->Release();
			_pStream = NULL;
		}

		if (_pFITSPropertyCache)
		{
			_pFITSPropertyCache->Release();
			_pFITSPropertyCache = NULL;
		}

		if (_pFITSInfo)
		{
			delete _pFITSInfo;
			_pFITSInfo = NULL;
		}

		if (_pSettings)
		{
			delete _pSettings;
			_pSettings = NULL;
		}

		DllRelease();
	}

	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
	{
		static const QITAB qit[] =
		{
			QITABENT(CFITSExplorerExtension, IInitializeWithStream),
			QITABENT(CFITSExplorerExtension, IThumbnailProvider),
			QITABENT(CFITSExplorerExtension, IPropertyStore),
			QITABENT(CFITSExplorerExtension, IPropertyStoreCapabilities),
			{ 0 },
		};
		return QISearch(this, qit, riid, ppv);
	}

	IFACEMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&_cRef);
	}

	IFACEMETHODIMP_(ULONG) Release()
	{
		ULONG cRef = InterlockedDecrement(&_cRef);
		if (!cRef)
		{
			delete this;
		}
		return cRef;
	}

	IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

	// IThumbnailProvider
	IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

	// IPropertyStore
	IFACEMETHODIMP GetCount(DWORD *pcProps);
	IFACEMETHODIMP GetAt(DWORD iProp, PROPERTYKEY *pkey);
	IFACEMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT *pPropVar);
	IFACEMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT propVar);
	IFACEMETHODIMP Commit();

	// IPropertyStoreCapabilities
	IFACEMETHODIMP IsPropertyWritable(REFPROPERTYKEY key);

private:
	HRESULT _LoadSettings();
	HRESULT _LoadFITSInfo();
	HRESULT _LoadFITSPropertyCache();

	Settings *_pSettings;

	long _cRef;
	IStream *_pStream;

	FITSInfo *_pFITSInfo;
	IPropertyStoreCache* _pFITSPropertyCache;
};

HRESULT CFITSExplorerExtension_CreateInstance(REFIID riid, void **ppv)
{
	CFITSExplorerExtension *pNew = new (std::nothrow) CFITSExplorerExtension();
	HRESULT hr = pNew ? S_OK : E_OUTOFMEMORY;
	if (SUCCEEDED(hr))
	{
		hr = pNew->QueryInterface(riid, ppv);
		pNew->Release();
	}
	return hr;
}

HRESULT CFITSExplorerExtension::_LoadSettings()
{
	HRESULT hr = E_FAIL;

	if (!_pSettings)
	{
		_pSettings = new (std::nothrow) Settings();
	}
	hr = _pSettings ? S_OK : E_OUTOFMEMORY;
	if (SUCCEEDED(hr))
	{
		DWORD value;

		hr = GetSettingsRegKeyValue(HKEY_CURRENT_USER, L"MaxInputSize", &value);
		if (SUCCEEDED(hr))
		{
			_pSettings->maxInputSize = value;
		}
		else
		{
			_pSettings->maxInputSize = DEFAULT_SETTINGS.maxInputSize;
			hr = S_OK;
		}

		hr = GetSettingsRegKeyValue(HKEY_CURRENT_USER, L"MaxOutputSize", &value);
		if (SUCCEEDED(hr))
		{
			_pSettings->maxOutputSize = value;
		}
		else
		{
			_pSettings->maxOutputSize = DEFAULT_SETTINGS.maxOutputSize;
			hr = S_OK;
		}

		hr = GetSettingsRegKeyValue(HKEY_CURRENT_USER, L"MaxThumbnailWidth", &value);
		if (SUCCEEDED(hr))
		{
			_pSettings->maxThumbnailWidth = value;
		}
		else
		{
			_pSettings->maxThumbnailWidth = DEFAULT_SETTINGS.maxThumbnailWidth;
			hr = S_OK;
		}

		hr = GetSettingsRegKeyValue(HKEY_CURRENT_USER, L"MaxThumbnailHeight", &value);
		if (SUCCEEDED(hr))
		{
			_pSettings->maxThumbnailHeight = value;
		}
		else
		{
			_pSettings->maxThumbnailHeight = DEFAULT_SETTINGS.maxThumbnailHeight;
			hr = S_OK;
		}

		hr = GetSettingsRegKeyValue(HKEY_CURRENT_USER, L"MonoColorOutline", &value);
		if (SUCCEEDED(hr))
		{
			_pSettings->monoColorOutline = value > 0;
		}
		else
		{
			_pSettings->monoColorOutline = DEFAULT_SETTINGS.monoColorOutline;
			hr = S_OK;
		}

		hr = GetSettingsRegKeyValue(HKEY_CURRENT_USER, L"Saturation", &value);
		if (SUCCEEDED(hr))
		{
			_pSettings->saturation = value;
		}
		else
		{
			_pSettings->saturation = DEFAULT_SETTINGS.saturation;
			hr = S_OK;
		}
	}

	return hr;
}

HRESULT CFITSExplorerExtension::_LoadFITSInfo()
{
	if (!_pStream)
	{
		return E_UNEXPECTED;
	}
	HRESULT hr = _LoadSettings();
	if (FAILED(hr))
	{
		return hr;
	}
	STATSTG stat;
	hr = _pStream->Stat(&stat, STATFLAG_NONAME);
	if (FAILED(hr))
	{
		return hr;
	}
	if (stat.cbSize.QuadPart > _pSettings->maxInputSize)
	{
		hr = E_FAIL;
		return hr;
	}
	if (!_pFITSInfo)
	{
		_pFITSInfo = new (std::nothrow) FITSInfo(_pStream, _pSettings->maxInputSize, _pSettings->maxThumbnailWidth, _pSettings->maxThumbnailHeight);
	}
	hr = _pFITSInfo ? S_OK : E_OUTOFMEMORY;
	if (SUCCEEDED(hr))
	{
		_pFITSInfo->ReadHeader();

		if (_pFITSInfo->isHeaderValid())
		{
			FITSImageDim dim = _pFITSInfo->outDim();
			if (dim.nx <= 0 || dim.ny <= 0 || dim.nc <= 0)
			{
				hr = E_FAIL;
			}
		}
		else
		{
			hr = E_FAIL;
		}
	}
	return hr;
}

HRESULT CFITSExplorerExtension::_LoadFITSPropertyCache()
{
	HRESULT hr = _LoadFITSInfo();
	if (SUCCEEDED(hr))
	{
		if (!_pFITSPropertyCache)
		{
			hr = PSCreateMemoryPropertyStore(IID_PPV_ARGS(&_pFITSPropertyCache));
			if (SUCCEEDED(hr))
			{
				PROPVARIANT propvarContent = {};

				hr = InitPropVariantFromUInt32(_pFITSInfo->inDim().nx, &propvarContent);
				if (SUCCEEDED(hr))
				{
					hr = _pFITSPropertyCache->SetValueAndState(PKEY_Image_HorizontalSize, &propvarContent, PSC_NORMAL);
					PropVariantClear(&propvarContent);
				}

				hr = InitPropVariantFromUInt32(_pFITSInfo->inDim().ny, &propvarContent);
				if (SUCCEEDED(hr))
				{
					hr = _pFITSPropertyCache->SetValueAndState(PKEY_Image_VerticalSize, &propvarContent, PSC_NORMAL);
					PropVariantClear(&propvarContent);
				}

				hr = InitPropVariantFromUInt32(abs(_pFITSInfo->storageType().fitsBitdepth), &propvarContent);
				if (SUCCEEDED(hr))
				{
					hr = _pFITSPropertyCache->SetValueAndState(PKEY_Image_BitDepth, &propvarContent, PSC_NORMAL);
					PropVariantClear(&propvarContent);
				}

				WCHAR szDimensions[32];
				szDimensions[0] = '\0';
				hr = StringCchPrintf(szDimensions, ARRAYSIZE(szDimensions), L"%d x %d", _pFITSInfo->inDim().nx, _pFITSInfo->inDim().ny);
				if (SUCCEEDED(hr))
				{
					hr = InitPropVariantFromString(szDimensions, &propvarContent);
					if (SUCCEEDED(hr))
					{
						hr = _pFITSPropertyCache->SetValueAndState(PKEY_Image_Dimensions, &propvarContent, PSC_NORMAL);
						PropVariantClear(&propvarContent);
					}
				}

				if (_pFITSInfo->date().year != 0)
				{
					SYSTEMTIME captureSystemTime;
					captureSystemTime.wYear = _pFITSInfo->date().year;
					captureSystemTime.wMonth = _pFITSInfo->date().month;
					captureSystemTime.wDay = _pFITSInfo->date().day;
					captureSystemTime.wHour = _pFITSInfo->date().hour;
					captureSystemTime.wMinute = _pFITSInfo->date().minute;
					captureSystemTime.wSecond = (DWORD)_pFITSInfo->date().second;

					FILETIME captureFileTime;
					if (SystemTimeToFileTime(&captureSystemTime, &captureFileTime))
					{
						hr = InitPropVariantFromFileTime(&captureFileTime, &propvarContent);
						if (SUCCEEDED(hr))
						{
							hr = _pFITSPropertyCache->SetValueAndState(PKEY_Photo_DateTaken, &propvarContent, PSC_NORMAL);
							PropVariantClear(&propvarContent);
						}
					}
				}

				if (_pFITSInfo->expTime() > 0)
				{
					hr = InitPropVariantFromDouble((double)_pFITSInfo->expTime(), &propvarContent);
					if (SUCCEEDED(hr))
					{
						hr = _pFITSPropertyCache->SetValueAndState(PKEY_Photo_ExposureTime, &propvarContent, PSC_NORMAL);
						PropVariantClear(&propvarContent);
					}
				}

				if (_pFITSInfo->focalLength() > 0)
				{
					hr = InitPropVariantFromDouble((double)_pFITSInfo->focalLength(), &propvarContent);
					if (SUCCEEDED(hr))
					{
						hr = _pFITSPropertyCache->SetValueAndState(PKEY_Photo_FocalLength, &propvarContent, PSC_NORMAL);
						PropVariantClear(&propvarContent);
					}
				}

				if (_pFITSInfo->aperture() > 0)
				{
					WCHAR szAperture[32];
					szAperture[0] = '\0';
					hr = StringCchPrintf(szAperture, ARRAYSIZE(szAperture), L"%d", (int)_pFITSInfo->aperture());
					if (SUCCEEDED(hr))
					{
						hr = InitPropVariantFromString(szAperture, &propvarContent);
						if (SUCCEEDED(hr))
						{
							hr = _pFITSPropertyCache->SetValueAndState(PKEY_Photo_Aperture, &propvarContent, PSC_NORMAL);
							PropVariantClear(&propvarContent);
						}
					}
				}
			}
		}
	}

	return hr;
}

IFACEMETHODIMP CFITSExplorerExtension::Initialize(IStream *pStream, DWORD)
{
	HRESULT hr = E_UNEXPECTED; // can only be initialized once
	if (!_pStream)
	{
		hr = pStream->QueryInterface(&_pStream);
	}
	return hr;
}

IFACEMETHODIMP CFITSExplorerExtension::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
	*phbmp = NULL;
	*pdwAlpha = WTSAT_RGB;

	HRESULT hr = _LoadSettings();
	if (FAILED(hr))
	{
		return hr;
	}

	hr = _LoadFITSInfo();
	if (FAILED(hr))
	{
		return hr;
	}

	FITSImageDim dim = _pFITSInfo->outDim();

	if ((size_t)dim.nx * (size_t)dim.ny * (size_t)16 > _pSettings->maxOutputSize)
	{
		hr = E_FAIL;
	}
	else
	{
		BITMAPINFOHEADER bmih;
		bmih.biSize = sizeof(BITMAPINFOHEADER);
		bmih.biWidth = dim.nx;
		bmih.biHeight = -dim.ny;
		bmih.biPlanes = 1;
		bmih.biBitCount = 32;
		bmih.biCompression = BI_RGB;
		bmih.biSizeImage = dim.nx * dim.ny * 4;
		bmih.biXPelsPerMeter = 10;
		bmih.biYPelsPerMeter = 10;
		bmih.biClrUsed = 0;
		bmih.biClrImportant = 0;

		BITMAPINFO dbmi;
		ZeroMemory(&dbmi, sizeof(dbmi));
		dbmi.bmiHeader = bmih;
		dbmi.bmiColors->rgbBlue = 0;
		dbmi.bmiColors->rgbGreen = 0;
		dbmi.bmiColors->rgbRed = 0;
		dbmi.bmiColors->rgbReserved = 0;

		void *bits = NULL;
		HBITMAP hbmp = CreateDIBSection(NULL, &dbmi, DIB_RGB_COLORS, &bits, NULL, 0);

		unsigned char *data = (unsigned char*)bits;

		FITSImageReadProps props;
		props.monoColorOutline = _pSettings->monoColorOutline;
		props.saturation = _pSettings->saturation / 1024.0f;

		if (!_pFITSInfo->ReadImage(data, props))
		{
			hr = E_FAIL;
		}

		if (SUCCEEDED(hr))
		{
			*phbmp = hbmp;
		}
	}

	return hr;
}

HRESULT CFITSExplorerExtension::GetCount(DWORD *pcProps)
{
	*pcProps = 0;
	_LoadFITSPropertyCache();
	return _pFITSPropertyCache ? _pFITSPropertyCache->GetCount(pcProps) : E_UNEXPECTED;
}

HRESULT CFITSExplorerExtension::GetAt(DWORD iProp, PROPERTYKEY *pkey)
{
	*pkey = PKEY_Null;
	_LoadFITSPropertyCache();
	return _pFITSPropertyCache ? _pFITSPropertyCache->GetAt(iProp, pkey) : E_UNEXPECTED;
}

HRESULT CFITSExplorerExtension::GetValue(REFPROPERTYKEY key, PROPVARIANT *pPropVar)
{
	PropVariantInit(pPropVar);
	_LoadFITSPropertyCache();
	return _pFITSPropertyCache ? _pFITSPropertyCache->GetValue(key, pPropVar) : E_UNEXPECTED;
}

HRESULT CFITSExplorerExtension::SetValue(REFPROPERTYKEY key, REFPROPVARIANT propVar)
{
	// Modification not allowed
	return E_UNEXPECTED;
}

HRESULT CFITSExplorerExtension::Commit()
{
	// Modification not allowed
	return E_UNEXPECTED;
}

HRESULT CFITSExplorerExtension::IsPropertyWritable(REFPROPERTYKEY key)
{
	// Modification not allowed
	return S_FALSE;
}
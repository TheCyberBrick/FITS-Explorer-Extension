#pragma once

#include <winnt.h>
#include <stdint.h>

class __declspec(uuid("98f32316-5ee8-4424-a39c-fa4e23720efd")) CFITSExplorerExtension;

struct PropertyList
{
	PCWSTR pszName;
	PCWSTR pszValue;
};

const LPWSTR SZ_PROG_ID = L"FITSExplorerExtension";
const LPWSTR SZ_PROG_NAME = L"FITS Explorer Extension";
const LPWSTR SZ_FILE_EXT[] = { L"fit", L"fits", L"fts" };

const PropertyList PROPERTY_LISTS[] = {
	{ L"InfoTip",            L"prop:System.Photo.DateTaken;System.Image.Dimensions;System.Image.BitDepth;System.Photo.ExposureTime;System.Photo.FocalLength;System.Photo.Aperture;System.Size" },
	{ L"FullDetails",        L"prop:System.PropGroup.Origin;System.Photo.DateTaken;System.PropGroup.Image;System.Image.HorizontalSize;System.Image.VerticalSize;System.Image.BitDepth;System.Image.Dimensions;System.Photo.ExposureTime;System.Photo.FocalLength;System.Photo.Aperture;System.PropGroup.FileSystem;System.FileName;System.ItemTypeText;System.ItemFolderPathDisplay;System.DateCreated;System.DateModified;System.Size;System.FileAttributes;System.OfflineAvailability;System.OfflineStatus;System.SharedWith;System.FileOwner;System.ComputerName" },
	{ L"PreviewDetails",     L"prop:System.Photo.DateTaken;System.Image.Dimensions;prop:System.Image.Dimensions;System.Image.BitDepth;System.Photo.ExposureTime;System.Photo.FocalLength;System.Photo.Aperture;System.Size;System.DateCreated" }
};

struct Settings
{
	size_t maxInputSize;
	size_t maxOutputSize;
	uint32_t maxThumbnailWidth;
	uint32_t maxThumbnailHeight;
	bool monoColorOutline;
	uint32_t saturation;
};

const Settings DEFAULT_SETTINGS = {
	8192 * 8192 * 3 * 4 /* x * y * rgb * bytes per component*/,
	8192 * 8192 * 3 * 4 /* x * y * rgb * bytes per component*/,
	512,
	512,
	true,
	1536 /*1536 / 1024 = 1.5*/
};
#pragma once

#include <winnt.h>

class __declspec(uuid("98f32316-5ee8-4424-a39c-fa4e23720efd")) CFITSExplorerExtension;

struct PROPERTYLIST
{
	PCWSTR pszName;
	PCWSTR pszValue;
};

const LPWSTR SZ_PROG_ID = L"FITSExplorerExtension";
const LPWSTR SZ_PROG_NAME = L"FITS Explorer Extension";
const LPWSTR SZ_FILE_EXT[] = { L"fit", L"fits" };

const PROPERTYLIST PROPERTY_LISTS[] = {
	{ L"InfoTip",            L"prop:System.Image.Dimensions;System.Image.BitDepth;System.Photo.ExposureTime;System.Photo.FocalLength;System.Photo.Aperture;System.Size" },
	{ L"FullDetails",        L"prop:System.PropGroup.Origin;System.Photo.DateTaken;System.PropGroup.Image;System.Image.HorizontalSize;System.Image.VerticalSize;System.Image.BitDepth;System.Image.Dimensions;System.Photo.ExposureTime;System.Photo.FocalLength;System.Photo.Aperture;System.PropGroup.FileSystem;System.FileName;System.ItemTypeText;System.ItemFolderPathDisplay;System.DateCreated;System.DateModified;System.Size;System.FileAttributes;System.OfflineAvailability;System.OfflineStatus;System.SharedWith;System.FileOwner;System.ComputerName" },
	{ L"PreviewDetails",     L"prop:System.Photo.DateTaken;System.Image.Dimensions;prop:System.Image.Dimensions;System.Image.BitDepth;System.Photo.ExposureTime;System.Photo.FocalLength;System.Photo.Aperture;System.Size;System.DateCreated" }
};
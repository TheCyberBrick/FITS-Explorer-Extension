#pragma once

typedef struct
{
	int fitsImgType;
	int fitsDatatype;
	int fitsBitdepth;
	size_t size;
	bool isSigned;
} FITSDatatype;

FITSDatatype GetFITSDatatypeFromImgType(int fitsImgType);
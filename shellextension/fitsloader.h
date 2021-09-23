#pragma once

#include <shlwapi.h>

#include <valarray>
#include <string>
#include <iostream>
#include <mutex>
#include <map>
#include <array>

#include "fitsio.h"
#include "fitstypeutil.h"

typedef struct
{
	int nx;
	int ny;
	int nc;
	uint32_t n;
} FITSImageDim;

typedef struct
{
	bool monoColorOutline;
} FITSImageReadProps;

enum class FITSColorFilter
{
	L = 0,
	R = 1,
	G = 2,
	B = 3,
	Other = 4
};

typedef struct
{
	uint16_t year;
	uint8_t month;
	uint16_t day;
	uint8_t hour;
	uint8_t minute;
	double second;
	bool hasTime;
} FITSDate;

class FITSInfo
{
public:
	// TODO Get rid of mutex and make CFITSIO stub driver
	// thread safe
	static std::mutex s_cfitsio_mutex;

	FITSInfo(IStream *stream, size_t maxInputSize, int maxWidth, int maxHeight);
	~FITSInfo();

	int maxWidth() { return _maxWidth; };
	int maxHeight() { return _maxHeight; };
	size_t maxInputSize() { return _maxInputSize; };
	bool isHeaderValid() { return _isHeaderValid; };
	FITSImageDim inDim() { return _imgDim; };
	FITSImageDim outDim() { return _outDim; };
	FITSDatatype storageType() { return _storageType; };
	FITSDatatype readType() { return _readType; };
	std::string bayerPattern() { return _bayerPattern; };
	bool hasCfa() { return _hasCfa; };
	std::array<float, 12> cfa() { return _cfa; };
	std::string filter() { return _filter; };
	FITSColorFilter colorFilter() { return _colorFilter; };
	float expTime() { return _expTime; };
	float focalLength() { return _focalLength; };
	float gain() { return _gain; };
	float aperture() { return _aperture; };
	std::string objectDec() { return _objectDec; };
	std::string objectRa() { return _objectRa; };
	FITSDate date() { return _date; };

	std::map<std::string, FITSColorFilter> colorFilters;
	std::map<std::string, std::array<float, 12>> bayerPatterns;

	void ReadHeader();

	bool ReadImage(unsigned char *data, FITSImageReadProps props);

private:
	IStream *_stream;
	fitsfile *_fitsFile;
	uint64_t _streamPos;
	bool _isHeaderValid;

	int _maxWidth;
	int _maxHeight;
	size_t _maxInputSize;
	FITSDatatype _storageType;
	FITSDatatype _readType;
	float _kernelSize;
	int _kernelStride;
	std::string _bayerPattern;
	bool _hasCfa;
	std::array<float, 12> _cfa;
	FITSImageDim _imgDim;
	FITSImageDim _outDim;
	std::string _filter;
	FITSColorFilter _colorFilter;
	float _expTime;
	float _focalLength;
	float _gain;
	float _aperture;
	std::string _objectDec;
	std::string _objectRa;
	FITSDate _date;

	int _downsampleFactor;

	int ReadStringKeyword(char *key, std::string *str, int *status);
	int ReadIntKeyword(char *key, int *value, int *status);
	int ReadFloatKeyword(char *key, float *value, int *status);
	int ReadDateKeyword(char *key, FITSDate *value, int *status);

	template<typename T_IN, typename T_OUT>
	bool ReadImage(int fits_datatype, bool issigned, unsigned char *data, FITSImageReadProps props);

	template<typename T>
	void ProcessImage(std::valarray<T>& data);

	template<typename T>
	void StoreImageBGRA32(std::valarray<T>& inData, unsigned char *outData, FITSImageReadProps props);

	void SetDriver();

	static int DriverOpen(void *instance, char *filename, int rwmode, int *handle);
	static int DriverRead(void *instance, int handle, void *buffer, long nbytes);
	static int DriverSize(void *instance, int handle, LONGLONG *filesize);
	static int DriverSeek(void *instance, int handle, LONGLONG offset);
	static int DriverCreate(void *instance, char *filename, int *handle);
	static int DriverWrite(void *instance, int hdl, void *buffer, long nbytes);
	static int DriverFlush(void *instance, int handle);
	static int DriverClose(void *instance, int handle);
};
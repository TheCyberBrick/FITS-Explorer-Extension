#include <stdio.h>
#include <cstdio>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cctype>

#include "stretch.h"
#include "fitsloader.h"
#include "fitsiodrvrstub.h"
#include "kerneldataiterator.h"
#include "hsv.h"

std::mutex FITSInfo::s_cfitsio_mutex;

FITSInfo::FITSInfo(IStream *stream, size_t maxInputSize, int maxWidth, int maxHeight) :
	_stream(stream), _colorFilter(FITSColorFilter::Other), _maxInputSize(maxInputSize),
	_maxWidth(maxWidth), _maxHeight(maxHeight)
{
	colorFilters["l"] = FITSColorFilter::L;
	colorFilters["lum"] = FITSColorFilter::L;
	colorFilters["luminance"] = FITSColorFilter::L;
	colorFilters["r"] = FITSColorFilter::R;
	colorFilters["red"] = FITSColorFilter::R;
	colorFilters["g"] = FITSColorFilter::G;
	colorFilters["green"] = FITSColorFilter::G;
	colorFilters["b"] = FITSColorFilter::B;
	colorFilters["blue"] = FITSColorFilter::B;

	bayerPatterns["rggb"] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	bayerPatterns["bggr"] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 0.5f, 0.5f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f
	};
	bayerPatterns["gbrg"] = {
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.0f, 0.0f, 0.5f,
		0.0f, 1.0f, 0.0f, 0.0f
	};
	bayerPatterns["grbg"] = {
		0.0f, 1.0f, 0.0f, 0.0f,
		0.5f, 0.0f, 0.0f, 0.5f,
		0.0f, 0.0f, 1.0f, 0.0f
	};
}

FITSInfo::~FITSInfo()
{
	int status(0);
	fits_close_file(_fitsFile, &status);
}

int FITSInfo::ReadStringKeyword(char *key, std::string *str, int *status)
{
	*str = "";
	char buffer[FLEN_VALUE];
	buffer[0] = '\0';
	*status = 0;
	fits_read_key(_fitsFile, TSTRING, key, buffer, NULL, status);
	if (strlen(buffer) > 0)
	{
		*str = std::string(buffer);
	}
	return *status;
}

int FITSInfo::ReadIntKeyword(char *key, int *value, int *status)
{
	*value = 0;
	*status = 0;
	fits_read_key(_fitsFile, TINT, key, value, NULL, status);
	return *status;
}

int FITSInfo::ReadFloatKeyword(char *key, float *value, int *status)
{
	*value = 0;
	*status = 0;
	fits_read_key(_fitsFile, TFLOAT, key, value, NULL, status);
	return *status;
}

int FITSInfo::ReadDateKeyword(char *key, FITSDate *value, int *status)
{
	value->year = 0;
	value->month = 0;
	value->day = 0;
	value->hour = 0;
	value->minute = 0;
	value->second = 0;
	value->hasTime = false;
	std::string dateStr;
	if (ReadStringKeyword(key, &dateStr, status) == 0)
	{
		int year = 0, month = 0, day = 0, hour = 0, minute = 0;
		double second = 0;
		if(fits_str2time(const_cast<char*>(dateStr.c_str()), &year, &month, &day, &hour, &minute, &second, status) == 0)
		{
			value->year = year;
			value->month = month;
			value->day = day;
			value->hour = hour;
			value->minute = minute;
			value->second = second;
			value->hasTime = true;
		}
		else if(fits_str2date(const_cast<char*>(dateStr.c_str()), &year, &month, &day, status) == 0)
		{
			value->year = year;
			value->month = month;
			value->day = day;
			value->hasTime = false;
		}
	}
	return *status;
}

void FITSInfo::ReadHeader()
{
	_isHeaderValid = false;

	std::lock_guard<std::mutex> guard(FITSInfo::s_cfitsio_mutex);

	SetDriver();

	int status;

	status = 0;
	if (fits_open_diskfile(&_fitsFile, "stub://memory", READONLY, &status))
	{
		return;
	}

	//Using 64 as maximum HDU number because fits_get_num_hdus
	//cannot be used due to it reading the entire stream
	for (int hdu_num = 1; hdu_num <= 64; hdu_num++)
	{
		int hdu_type = 0;

		status = 0;
		if (fits_movabs_hdu(_fitsFile, hdu_num, &hdu_type, &status))
		{
			//EOF
			break;
		}

		if (hdu_type == IMAGE_HDU)
		{
			char valueBuffer[FLEN_VALUE];

			// ---- Read number of keys ----

			int num_keys;
			status = 0;
			if (fits_get_hdrspace(_fitsFile, &num_keys, NULL, &status))
			{
				break;
			}

			// ---- Read image depth (negative = float/double) ----

			int depth;
			status = 0;
			if (fits_get_img_type(_fitsFile, &depth, &status))
			{
				break;
			}

			// ---- Read image dimensions ----

			int dim;
			status = 0;
			if (fits_get_img_dim(_fitsFile, &dim, &status))
			{
				break;
			}

			// ---- Read image size ----

			std::vector<long> size(dim);
			status = 0;
			if (fits_get_img_size(_fitsFile, dim, size.data(), &status) || size.size() < 2)
			{
				break;
			}

			// ---- Read bayer pattern ----

			if (ReadStringKeyword("BAYERPAT", &_bayerPattern, &status) != 0)
			{
				if (ReadStringKeyword("COLORTYP", &_bayerPattern, &status) != 0)
				{
					ReadStringKeyword("COLORTYPE", &_bayerPattern, &status);
				}
			}
			std::transform(_bayerPattern.begin(), _bayerPattern.end(), _bayerPattern.begin(), std::tolower);
			if (bayerPatterns.find(_bayerPattern) != bayerPatterns.end())
			{
				_cfa = bayerPatterns[_bayerPattern];
				_hasCfa = true;
			}
			else
			{
				_hasCfa = false;
			}

			// ---- Store image dim ----

			_imgDim.nx = static_cast<int>(size[0]);
			_imgDim.ny = static_cast<int>(size[1]);
			_imgDim.nc = size.size() == 3 ? 3 : 1;
			_imgDim.depth = depth;
			_imgDim.n = (uint32_t)_imgDim.nx * (uint32_t)_imgDim.ny * (uint32_t)_imgDim.nc;

			int width = _imgDim.nx;
			int height = _imgDim.ny;

			if (_imgDim.nc == 1 && _hasCfa)
			{
				// bayered data, output will be half size
				width /= 2;
				height /= 2;
			}

			float downsampleX = (float)width / (float)_maxWidth;
			float downsampleY = (float)height / (float)_maxHeight;

			int constrainedDim = -1;

			// determine smallest kernel size that s.t. no pixels
			// are ignored when downsampling
			_kernelSize = 0;
			if (downsampleX > downsampleY && downsampleX > 1)
			{
				_kernelSize = (float)(_imgDim.nx - _maxWidth) / (float)(2 * (_maxWidth + 1));
				constrainedDim = 0;
			}
			else if (downsampleY > downsampleX && downsampleY > 1)
			{
				_kernelSize = (float)(_imgDim.ny - _maxHeight) / (float)(2 * (_maxHeight + 1));
				constrainedDim = 1;
			}
			else
			{
				_kernelSize = 0;
			}

			// determine smallest kernel stride s.t. the output
			// image fits within the min and max height
			_kernelStride = 1 + 2 * (int)ceil(_kernelSize);
			if (_kernelSize > 0)
			{
				if (constrainedDim == 0)
				{
					_kernelStride = ceil((float)(width - 2 * (int)ceil(_kernelSize)) / (float)_maxWidth);
				}
				else if (constrainedDim == 1)
				{
					_kernelStride = ceil((float)(height - 2 * (int)ceil(_kernelSize)) / (float)_maxHeight);
				}
			}

			int outWidth = (width - 2 * (int)ceil(_kernelSize)) / _kernelStride;
			int outHeight = (height - 2 * (int)ceil(_kernelSize)) / _kernelStride;

			if (_imgDim.nc == 1 && !_hasCfa)
			{
				_outDim = { outWidth, outHeight, 1, 8 };
			}
			else
			{
				_outDim = { outWidth, outHeight, 3, 8 };
			}
			_outDim.n = _outDim.nx * _outDim.ny * _outDim.nc;

			// ---- Read filter ----

			ReadStringKeyword("FILTER", &_filter, &status);
			if (_filter.length() == 0)
			{
				int filterCount = -1;
				if (!(ReadIntKeyword("FILTNUM", &filterCount, &status) == 0 || ReadIntKeyword("FILTNUMBER", &filterCount, &status) == 0 || ReadIntKeyword("FILTNR", &filterCount, &status) == 0 || ReadIntKeyword("FILTN", &filterCount, &status) == 0
					|| ReadIntKeyword("FILTERNUM", &filterCount, &status) == 0 || ReadIntKeyword("FILTERNUMBER", &filterCount, &status) == 0 || ReadIntKeyword("FILTERNR", &filterCount, &status) == 0 || ReadIntKeyword("FILTERN", &filterCount, &status) == 0))
				{
					// No filter count specified, assume 1
					filterCount = 1;
				}
				if(filterCount == 1)
				{
					std::string tmpFilter = "";
					std::string tmpFilterDiscard = "";
					if ((ReadStringKeyword("FILT0", &tmpFilter, &status) == 0 || ReadStringKeyword("FILT-0", &tmpFilter, &status) == 0 || ReadStringKeyword("FILTER0", &tmpFilter, &status) == 0 || ReadStringKeyword("FILTER-0", &tmpFilter, &status) == 0) && (ReadStringKeyword("FILT1", &tmpFilterDiscard, &status) != 0 && ReadStringKeyword("FILT-1", &tmpFilterDiscard, &status) != 0 && ReadStringKeyword("FILTER1", &tmpFilterDiscard, &status) != 0 && ReadStringKeyword("FILTER-1", &tmpFilterDiscard, &status) != 0)
						|| (ReadStringKeyword("FILT0", &tmpFilterDiscard, &status) != 0 || ReadStringKeyword("FILT-0", &tmpFilterDiscard, &status) != 0 || ReadStringKeyword("FILTER0", &tmpFilterDiscard, &status) != 0 || ReadStringKeyword("FILTER-0", &tmpFilterDiscard, &status) != 0) && (ReadStringKeyword("FILT1", &tmpFilter, &status) == 0 || ReadStringKeyword("FILT-1", &tmpFilter, &status) == 0 || ReadStringKeyword("FILTER1", &tmpFilter, &status) == 0 || ReadStringKeyword("FILTER-1", &tmpFilter, &status) == 0) && (ReadStringKeyword("FILT2", &tmpFilterDiscard, &status) != 0 && ReadStringKeyword("FILT-2", &tmpFilterDiscard, &status) != 0 && ReadStringKeyword("FILTER2", &tmpFilterDiscard, &status) != 0 && ReadStringKeyword("FILTER-2", &tmpFilterDiscard, &status) != 0))
					{
						_filter = tmpFilter;
					}
				}
			}
			if (_filter.length() > 0)
			{
				std::string filterLCase = _filter;
				std::transform(filterLCase.begin(), filterLCase.end(), filterLCase.begin(), std::tolower);
				if (colorFilters.find(filterLCase) != colorFilters.end())
				{
					_colorFilter = colorFilters[filterLCase];
				}
			}

			// ---- Read exposure time ----

			if (ReadFloatKeyword("EXPOSURE", &_expTime, &status) != 0)
			{
				if (ReadFloatKeyword("EXPTIME", &_expTime, &status) != 0)
				{
					ReadFloatKeyword("EXP", &_expTime, &status);
				}
			}

			// ---- Read focal length ----

			if (ReadFloatKeyword("FOCALLEN", &_focalLength, &status) != 0)
			{
				ReadFloatKeyword("FOCALLENGTH", &_focalLength, &status);
			}

			// ---- Read gain ----

			ReadFloatKeyword("GAIN", &_gain, &status);

			// ---- Read aperture ----

			ReadFloatKeyword("APERTURE", &_aperture, &status);

			// ---- Read RA & DEC ----

			ReadStringKeyword("OBJCTDEC", &_objectDec, &status);
			ReadStringKeyword("OBJCTRA", &_objectRa, &status);

			// ---- Read date ----

			if (ReadDateKeyword("DATE-OBS", &_date, &status) != 0)
			{
				ReadDateKeyword("DATE", &_date, &status);
			}

			// ---- Read keys ----

			//Make sure to read all keys. According to cfitsio all keys must
			//have been read before reading an image if a stream driver is used.
			//Failing to read a record is ok - all that matters is that
			//all records have been read.
			char card[FLEN_CARD];
			for (int key = 1; key <= num_keys; key++)
			{
				status = 0;
				fits_read_record(_fitsFile, key, card, &status);
			}

			_isHeaderValid = true;

			break;
		}
	}
}

template<>
int FITSInfo::GetFitsDataType<uint16_t>()
{
	return TUSHORT;
}

template<>
int FITSInfo::GetFitsDataType<float>()
{
	return TFLOAT;
}

bool FITSInfo::ReadImage(unsigned char *data, FITSImageReadProps props)
{
	if (!_isHeaderValid || ((size_t)_imgDim.n * (size_t)abs(_imgDim.depth) / 8 > _maxInputSize))
	{
		return false;
	}

	std::lock_guard<std::mutex> guard(FITSInfo::s_cfitsio_mutex);

	SetDriver();

	if (_imgDim.depth == SHORT_IMG)
	{
		return ReadImage<uint16_t>(data, props);
	}
	else
	{
		return ReadImage<float>(data, props);
	}
}

template<typename T>
bool FITSInfo::ReadImage(unsigned char *data, FITSImageReadProps props)
{
	long fpixel(1);
	T nulval(0);
	int anynul(0);

	std::valarray<T> imgData(_outDim.n);

	int fullKernelSize = (int)ceil(_kernelSize);

	int const kernelDim = (1 + fullKernelSize * 2);

	std::valarray<float> weights(kernelDim * kernelDim);
	if (weights.size() == 1)
	{
		weights[0] = 1.0f;
	}
	else
	{
		// generate gaussian kernel
		float sigma = 0.5f * _kernelSize;
		float s = 2.0f * sigma * sigma;
		float sum = 0.0f;
		for (int x = -fullKernelSize; x <= fullKernelSize; x++) {
			for (int y = -fullKernelSize; y <= fullKernelSize; y++) {
				float r = sqrt((float)x * x + (float)y * y);
				weights[x + fullKernelSize + kernelDim * (y + fullKernelSize)] = (exp(-(r * r) / s)) / (3.1416f * s);
				sum += weights[x + fullKernelSize + kernelDim * (y + fullKernelSize)];
			}
		}
		for (int i = 0; i < kernelDim; ++i) {
			for (int j = 0; j < kernelDim; ++j) {
				weights[i + kernelDim * j] /= sum;
			}
		}
	}

	Kernel<T> kernel;
	kernel.size = fullKernelSize;
	kernel.stride = _kernelStride;
	kernel.weights = &weights[0];
	kernel.inputWidth = _imgDim.nx;
	kernel.inputHeight = _imgDim.ny;
	kernel.outData = &imgData[0];
	kernel.cfa = _hasCfa ? _cfa.data() : nullptr;
	kernel.rgb = _outDim.nc == 3;

	int status = 0;
	if (ReadData(_fitsFile, GetFitsDataType<T>(), &kernel, &status) != 0)
	{
		return false;
	}

	ProcessImage<T>(imgData);

	StoreImageBGRA32<T>(imgData, data, props);

	return true;
};

template<typename T>
void FITSInfo::ProcessImage(std::valarray<T>& data)
{
	StretchParams stretchParams;
	ComputeParamsAllChannels(data, &stretchParams, _outDim);
	StretchAllChannels(data, stretchParams, _outDim);
}

template<typename T>
void FITSInfo::StoreImageBGRA32(std::valarray<T>& inData, unsigned char *outData, FITSImageReadProps props)
{
	auto& size = _outDim;
	if (size.nc == 1)
	{
		if (props.monoColorOutline && _colorFilter != FITSColorFilter::Other)
		{
			int const border = std::max(2, std::min(size.nx / 50, size.ny / 50));

			switch (_colorFilter)
			{
			case FITSColorFilter::L:
				for (int y = 0; y < size.ny; y++)
				{
					for (int x = 0; x < size.nx; x++)
					{
						int i = (y * size.nx + x);

						unsigned char v = (unsigned char)(inData[i]);
						if (x <= border || x >= size.nx - 1 - border || y <= border || y >= size.ny - 1 - border)
						{
							outData[i * 4 + 0] = 200;
							outData[i * 4 + 1] = 200;
							outData[i * 4 + 2] = 200;
						}
						else
						{
							outData[i * 4 + 0] = v;
							outData[i * 4 + 1] = v;
							outData[i * 4 + 2] = v;
						}
						outData[i * 4 + 3] = 255;
					}
				}
				break;
			case FITSColorFilter::R:
				for (int y = 0; y < size.ny; y++)
				{
					for (int x = 0; x < size.nx; x++)
					{
						int i = (y * size.nx + x);

						unsigned char v = (unsigned char)(inData[i]);
						if (x <= border || x >= size.nx - 1 - border || y <= border || y >= size.ny - 1 - border)
						{
							outData[i * 4 + 0] = v * 0.2f;
							outData[i * 4 + 1] = v * 0.2f;
							outData[i * 4 + 2] = 200;
						}
						else
						{
							outData[i * 4 + 0] = v;
							outData[i * 4 + 1] = v;
							outData[i * 4 + 2] = v;
						}
						outData[i * 4 + 3] = 255;
					}
				}
				break;
			case FITSColorFilter::G:
				for (int y = 0; y < size.ny; y++)
				{
					for (int x = 0; x < size.nx; x++)
					{
						int i = (y * size.nx + x);

						unsigned char v = (unsigned char)(inData[i]);
						if (x <= border || x >= size.nx - 1 - border || y <= border || y >= size.ny - 1 - border)
						{
							outData[i * 4 + 0] = v * 0.2f;
							outData[i * 4 + 1] = 200;
							outData[i * 4 + 2] = v * 0.2f;
						}
						else
						{
							outData[i * 4 + 0] = v;
							outData[i * 4 + 1] = v;
							outData[i * 4 + 2] = v;
						}
						outData[i * 4 + 3] = 255;
					}
				}
				break;
			case FITSColorFilter::B:
				for (int y = 0; y < size.ny; y++)
				{
					for (int x = 0; x < size.nx; x++)
					{
						int i = (y * size.nx + x);

						unsigned char v = (unsigned char)(inData[i]);
						if (x <= border || x >= size.nx - border || y <= border || y >= size.ny - border)
						{
							outData[i * 4 + 0] = 200;
							outData[i * 4 + 1] = v * 0.2f;
							outData[i * 4 + 2] = v * 0.2f;
						}
						else
						{
							outData[i * 4 + 0] = v;
							outData[i * 4 + 1] = v;
							outData[i * 4 + 2] = v;
						}
						outData[i * 4 + 3] = 255;
					}
				}
				break;
			}
		}
		else
		{
			for (int i = 0; i < size.ny * size.nx; i++)
			{
				unsigned char v = (unsigned char)(inData[i]);
				outData[i * 4 + 0] = v;
				outData[i * 4 + 1] = v;
				outData[i * 4 + 2] = v;
				outData[i * 4 + 3] = 255;
			}
		}
	}
	else
	{
		int stride = size.nx * size.ny;

		for (int iny = 0; iny < size.ny; iny++)
		{
			for (int inx = 0; inx < size.nx; inx++)
			{
				int inIdx = (iny * size.nx + inx);
				int outIdx = inIdx * 4;

				RgbColor rgb;
				rgb.r = (unsigned char)(inData[inIdx]);
				rgb.g = (unsigned char)(inData[inIdx + stride]);
				rgb.b = (unsigned char)(inData[inIdx + stride * 2]);

				HsvColor hsv = RgbToHsv(rgb);

				hsv.s = std::min((unsigned char)(1.5f * hsv.s), (unsigned char)255);

				rgb = HsvToRgb(hsv);

				outData[outIdx + 2] = rgb.r;
				outData[outIdx + 1] = rgb.g;
				outData[outIdx + 0] = rgb.b;
				outData[outIdx + 3] = 255;
			}
		}
	}
}

void FITSInfo::SetDriver()
{
	fits_set_stub_driver_impl(this, &FITSInfo::DriverOpen, &FITSInfo::DriverRead, &FITSInfo::DriverSize,
		&FITSInfo::DriverSeek, &FITSInfo::DriverCreate, &FITSInfo::DriverWrite, &FITSInfo::DriverFlush,
		&FITSInfo::DriverClose);
}

int FITSInfo::DriverOpen(void *instance, char *filename, int rwmode, int *handle)
{
	return 0;
}

int FITSInfo::DriverRead(void *instance, int handle, void *buffer, long nbytes)
{
	FITSInfo* img = static_cast<FITSInfo*>(instance);

	ULONG nread;
	char *cptr;

	img->_stream->Read(buffer, nbytes, &nread);
	img->_streamPos += nread;

	if (nread == 1)
	{
		cptr = (char *)buffer;

		/* some editors will add a single end-of-file character to a file */
		/* Ignore it if the character is a zero, 10, or 32 */
		if (*cptr == 0 || *cptr == 10 || *cptr == 32)
		{
			return END_OF_FILE;
		}
		else
		{
			return READ_ERROR;
		}
	}
	else if (nread != nbytes)
	{
		return READ_ERROR;
	}

	return 0;
}

int FITSInfo::DriverSize(void *instance, int handle, LONGLONG *filesize)
{
	*filesize = LONGLONG_MAX;
	return 0;
}

int FITSInfo::DriverSeek(void *instance, int handle, LONGLONG position)
{
	FITSInfo* img = static_cast<FITSInfo*>(instance);

	uint64_t seek_offset = position - img->_streamPos;

	if (seek_offset > 0)
	{
		img->_stream->Seek({ (DWORD) seek_offset }, STREAM_SEEK_CUR, NULL);
		return 0;
	}
	else
	{
		//Cannot seek backwards in a stream
		return SEEK_ERROR;
	}
}

int FITSInfo::DriverCreate(void *instance, char *filename, int *handle)
{
	return FILE_NOT_CREATED;
}

int FITSInfo::DriverWrite(void *instance, int hdl, void *buffer, long nbytes)
{
	return WRITE_ERROR;
}

int FITSInfo::DriverFlush(void *instance, int handle)
{
	return 0;
}

int FITSInfo::DriverClose(void *instance, int handle)
{
	return 0;
}
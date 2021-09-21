#pragma once

#include "fitsio.h"

template<typename T>
struct IteratorData
{
	int outputWidth;
	int outputHeight;

	// input data from cfitsio
	T *inData;

	// 1 + 2 * kernelSize
	int kernelDim;

	// iteratorData.kernelDim - 1
	int processingStart;

	// kernelDim * width
	int bufferSize;

	// data buffer that holds bufferSize number of values
	// for the kernel processing
	std::shared_ptr<T> buffer;

	// buffer begin index
	int bufferProcessingIndex;

	// buffer write cursor
	int bufferIndex;
	int bufferRowIndex;

	// y coordinate counter for kernel stride
	int strideYCounter;

	// current output y coordinate
	int outY;

	IteratorData() :
		inData(nullptr), kernelDim(0), processingStart(0), bufferSize(0),
		buffer(nullptr), bufferProcessingIndex(0), bufferIndex(0),
		bufferRowIndex(0), outputWidth(0), outputHeight(0),
		strideYCounter(0), outY(0)
	{

	}
};

template<typename T>
struct Kernel
{
	// size and stride of kernel
	int size;
	int stride;

	// weights of kernel,
	// (1 + 2 * size) x (1 + 2 * size) row major matrix
	float *weights;

	// offset to be added to each output pixel
	T offset;

	// input data size
	int inputWidth, inputHeight;

	// whether the input image is RGB
	bool rgb;

	// cfa, 3 x 4 row major matrix,
	// rgb must be true for cfa to
	// take effect
	float *cfa;

	// output data pointer
	T *outData;

	IteratorData<T> it;

	Kernel() :
		size(0), stride(0), weights(0), offset(0), inputWidth(0), inputHeight(0),
		cfa(nullptr), rgb(false), outData(nullptr), it()
	{

	}
};

template<typename T>
int KernelDataIterator(long totaln, long offset, long firstn, long nvalues, int narrays, iteratorCol *data, void *userPointer)
{
	Kernel<T> *kernelPtr = (Kernel<T>*)userPointer;
	Kernel<T> kernel = *kernelPtr;
	IteratorData<T> iteratorData = kernel.it;

	if (firstn == 1)
	{
		// Initialize iterator

		if (narrays != 1)
		{
			return -1;
		}

		if (!kernel.rgb)
		{
			kernel.cfa = nullptr;
		}

		iteratorData.outputWidth = (kernel.inputWidth / (kernel.cfa ? 2 : 1) - 2 * kernel.size) / kernel.stride;
		iteratorData.outputHeight = (kernel.inputHeight / (kernel.cfa ? 2 : 1) - 2 * kernel.size) / kernel.stride;
		iteratorData.inData = (T*)fits_iter_get_array(&data[0]);
		iteratorData.kernelDim = (1 + 2 * kernel.size);
		iteratorData.processingStart = iteratorData.kernelDim * (kernel.cfa ? 2 : 1) - 1;
		iteratorData.bufferSize = iteratorData.kernelDim * (kernel.cfa ? 2 : 1) /*y*/ * kernel.inputWidth /*x*/;
		iteratorData.buffer = std::shared_ptr<T>(new T[iteratorData.bufferSize]);
		iteratorData.bufferProcessingIndex = 0;
		iteratorData.bufferIndex = 0;
		iteratorData.bufferRowIndex = 0;
		iteratorData.strideYCounter = 0;
		iteratorData.outY = 0;
	}

	T *outData = kernel.outData;
	T *buffer = iteratorData.buffer.get();

	// cfitsio data starts at 1. 0th element contains null value
	int inDataIndex = 1;

	bool finished = false;

	// copy data into buffer row by row
	int remaining = nvalues;
	while (remaining > 0)
	{
		// calculate number of values to be copied into buffer until none remain or buffer row becomes full
		int const count = std::min(iteratorData.bufferIndex + remaining, iteratorData.bufferRowIndex + kernel.inputWidth) - iteratorData.bufferIndex;

		// copy data into buffer and advance cursors
		memcpy(buffer + iteratorData.bufferIndex, iteratorData.inData + inDataIndex, count * sizeof(T));
		iteratorData.bufferIndex += count;
		inDataIndex += count;
		remaining -= count;

		// check if buffer row full
		if (iteratorData.bufferIndex == iteratorData.bufferRowIndex + kernel.inputWidth)
		{
			iteratorData.bufferIndex = iteratorData.bufferRowIndex = iteratorData.bufferIndex % iteratorData.bufferSize;

			// check if buffer is full enough to begin processing
			if (iteratorData.strideYCounter >= iteratorData.processingStart)
			{
				// check whether values need to be output (Y axis kernel stride)
				if ((iteratorData.strideYCounter - iteratorData.processingStart) % (kernel.stride * (kernel.cfa ? 2 : 1)) == 0)
				{
					float cfa[12];
					if (kernel.cfa)
					{
						memcpy(cfa, kernel.cfa, sizeof(cfa));
					}

					for (int outX = 0; outX < iteratorData.outputWidth; outX++)
					{
						int const kernelStart = outX * kernel.stride;

						if (!kernel.cfa)
						{
							// no cfa, can apply kernel directly
							float vsum = 0;
							for (int kernelY = 0; kernelY < iteratorData.kernelDim; kernelY++)
							{
								for (int kernelX = kernelStart; kernelX < kernelStart + iteratorData.kernelDim; kernelX++)
								{
									vsum += kernel.weights[kernelY * iteratorData.kernelDim + kernelX - kernelStart] * (float)buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX)) % iteratorData.bufferSize];
								}
							}
							outData[iteratorData.outY * iteratorData.outputWidth + outX] = (T)vsum + kernel.offset;
						}
						else
						{
							// need to calculate r, g and b through
							// cfa matrix and then apply kernel
							float rsum = 0;
							float gsum = 0;
							float bsum = 0;
							for (int kernelY = 0; kernelY < iteratorData.kernelDim; kernelY++)
							{
								for (int kernelX = kernelStart; kernelX < kernelStart + iteratorData.kernelDim; kernelX++)
								{
									float w = kernel.weights[kernelY * iteratorData.kernelDim + kernelX - kernelStart];
									float tl = w * (float)buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX) * 2 + 0 + 0 * kernel.inputWidth) % iteratorData.bufferSize];
									float tr = w * (float)buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX) * 2 + 1 + 0 * kernel.inputWidth) % iteratorData.bufferSize];
									float bl = w * (float)buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX) * 2 + 0 + 1 * kernel.inputWidth) % iteratorData.bufferSize];
									float br = w * (float)buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX) * 2 + 1 + 1 * kernel.inputWidth) % iteratorData.bufferSize];
									rsum += tl * cfa[0] + tr * cfa[1] + bl * cfa[2]  + br * cfa[3];
									gsum += tl * cfa[4] + tr * cfa[5] + bl * cfa[6]  + br * cfa[7];
									bsum += tl * cfa[8] + tr * cfa[9] + bl * cfa[10] + br * cfa[11];
								}
							}
							int const pixPerPlane = iteratorData.outputWidth * iteratorData.outputHeight;
							outData[iteratorData.outY * iteratorData.outputWidth + outX + 0 * pixPerPlane] = (T)rsum + kernel.offset;
							outData[iteratorData.outY * iteratorData.outputWidth + outX + 1 * pixPerPlane] = (T)gsum + kernel.offset;
							outData[iteratorData.outY * iteratorData.outputWidth + outX + 2 * pixPerPlane] = (T)bsum + kernel.offset;
						}
					}

					iteratorData.bufferProcessingIndex = iteratorData.bufferIndex;

					iteratorData.outY++;

					if (iteratorData.outY >= iteratorData.outputHeight * ((kernel.rgb && !kernel.cfa) ? 3 : 1))
					{
						finished = true;
						break;
					}
				}
			}

			iteratorData.strideYCounter++;

			// reset stride Y counter when next image plane is reached
			if (iteratorData.strideYCounter >= kernel.inputHeight)
			{
				iteratorData.strideYCounter = 0;
				if (kernel.size > 1)
				{
					iteratorData.outY--;
				}
			}
		}
	}

	kernel.it = iteratorData;
	*kernelPtr = kernel;

	if (finished)
	{
		return -1;
	}

	return 0;
}

template<typename T>
int ReadData(fitsfile *fptr, int datatype, Kernel<T> *kernel, int *status)
{
	iteratorCol cols[1];

	fits_iter_set_file(&cols[0], fptr);
	fits_iter_set_iotype(&cols[0], InputCol);
	fits_iter_set_datatype(&cols[0], datatype);

	fits_iterate_data(1, cols, 0, 0, &KernelDataIterator<T>, kernel, status);

	if (*status == -1)
	{
		*status = 0;
	}

	return *status;
}
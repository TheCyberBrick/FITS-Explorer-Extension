#pragma once

#include "fitsio.h"

template<typename T_IN, typename T_OUT>
struct IteratorData
{
	int outputWidth;
	int outputHeight;

	// input data from cfitsio
	T_IN *inData;

	// 1 + 2 * kernelSize
	int kernelDim;

	// iteratorData.kernelDim - 1
	int processingStart;

	// kernelDim * width
	int bufferSize;

	// data buffer that holds bufferSize number of values
	// for the kernel processing
	std::shared_ptr<T_IN> buffer;

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

template<typename T_IN, typename T_OUT>
struct Kernel
{
	// size and stride of kernel
	int size;
	int stride;

	// weights of kernel,
	// (1 + 2 * size) x (1 + 2 * size) row major matrix
	float *weights;

	// offset to be added to each pixel
	T_OUT offset;

	// input data size
	int inputWidth, inputHeight;

	// whether the input image is RGB
	bool rgb;

	// cfa, 3 x 4 row major matrix,
	// rgb must be true for cfa to
	// take effect
	float *cfa;

	// output data pointer
	T_OUT *outData;

	// pointer to bool that will be set to true
	// if the data contained negative values
	bool *hasNegative;

	IteratorData<T_IN, T_OUT> it;

	Kernel() :
		size(0), stride(0), weights(nullptr), offset(0),
		inputWidth(0), inputHeight(0), cfa(nullptr),
		outData(nullptr), hasNegative(false), it()
	{

	}
};

template<typename T_IN, typename T_OUT>
int KernelDataIterator(long totaln, long offset, long firstn, long nvalues, int narrays, iteratorCol *data, void *userPointer)
{
	Kernel<T_IN, T_OUT> *kernelPtr = (Kernel<T_IN, T_OUT>*)userPointer;
	Kernel<T_IN, T_OUT> kernel = *kernelPtr;
	IteratorData<T_IN, T_OUT> iteratorData = kernel.it;

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
		iteratorData.inData = (T_IN*)fits_iter_get_array(&data[0]);
		iteratorData.kernelDim = (1 + 2 * kernel.size);
		iteratorData.processingStart = iteratorData.kernelDim * (kernel.cfa ? 2 : 1) - 1;
		iteratorData.bufferSize = iteratorData.kernelDim * (kernel.cfa ? 2 : 1) /*y*/ * kernel.inputWidth /*x*/;
		iteratorData.buffer = std::shared_ptr<T_IN>(new T_IN[iteratorData.bufferSize]);
		iteratorData.bufferProcessingIndex = 0;
		iteratorData.bufferIndex = 0;
		iteratorData.bufferRowIndex = 0;
		iteratorData.strideYCounter = 0;
		iteratorData.outY = 0;
	}

	T_OUT *outData = kernel.outData;
	T_IN *buffer = iteratorData.buffer.get();

	// cfitsio data starts at 1. 0th element contains null value
	int inDataIndex = 1;

	bool finished = false;
	bool hasNegative = false;

	// copy data into buffer row by row
	int remaining = nvalues;
	while (remaining > 0)
	{
		// calculate number of values to be copied into buffer until none remain or buffer row becomes full
		int const count = std::min(iteratorData.bufferIndex + remaining, iteratorData.bufferRowIndex + kernel.inputWidth) - iteratorData.bufferIndex;

		// copy data into buffer and advance cursors
		memcpy(buffer + iteratorData.bufferIndex, iteratorData.inData + inDataIndex, count * sizeof(T_IN));
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
									T_IN v = buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX)) % iteratorData.bufferSize];
									hasNegative |= v < 0;
									vsum += kernel.weights[kernelY * iteratorData.kernelDim + kernelX - kernelStart] * v;
								}
							}
							outData[iteratorData.outY * iteratorData.outputWidth + outX] = (T_OUT)(vsum + kernel.offset);
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
									T_IN tl = buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX) * 2 + 0 + 0 * kernel.inputWidth) % iteratorData.bufferSize];
									T_IN tr = buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX) * 2 + 1 + 0 * kernel.inputWidth) % iteratorData.bufferSize];
									T_IN bl = buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX) * 2 + 0 + 1 * kernel.inputWidth) % iteratorData.bufferSize];
									T_IN br = buffer[(iteratorData.bufferProcessingIndex + (kernelY * kernel.inputWidth + kernelX) * 2 + 1 + 1 * kernel.inputWidth) % iteratorData.bufferSize];
									hasNegative |= tl < 0 || tr < 0 || bl < 0 || br < 0;
									float tlw = w * tl;
									float trw = w * tr;
									float blw = w * bl;
									float brw = w * br;
									rsum += tlw * cfa[0] + trw * cfa[1] + blw * cfa[2]  + brw * cfa[3];
									gsum += tlw * cfa[4] + trw * cfa[5] + blw * cfa[6]  + brw * cfa[7];
									bsum += tlw * cfa[8] + trw * cfa[9] + blw * cfa[10] + brw * cfa[11];
								}
							}
							int const pixPerPlane = iteratorData.outputWidth * iteratorData.outputHeight;
							outData[iteratorData.outY * iteratorData.outputWidth + outX + 0 * pixPerPlane] = (T_OUT)(rsum + kernel.offset);
							outData[iteratorData.outY * iteratorData.outputWidth + outX + 1 * pixPerPlane] = (T_OUT)(gsum + kernel.offset);
							outData[iteratorData.outY * iteratorData.outputWidth + outX + 2 * pixPerPlane] = (T_OUT)(bsum + kernel.offset);
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

	if (kernel.hasNegative)
	{
		*kernel.hasNegative = hasNegative;
	}

	kernelPtr->it = iteratorData;

	if (finished)
	{
		return -1;
	}

	return 0;
}

template<typename T_IN, typename T_OUT>
int ReadData(fitsfile *fptr, int datatype, Kernel<T_IN, T_OUT> *kernel, int *status)
{
	iteratorCol cols[1];

	fits_iter_set_file(&cols[0], fptr);
	fits_iter_set_iotype(&cols[0], InputCol);
	fits_iter_set_datatype(&cols[0], datatype);

	fits_iterate_data(1, cols, 0, 0, &KernelDataIterator<T_IN, T_OUT>, kernel, status);

	if (*status == -1)
	{
		*status = 0;
	}

	return *status;
}
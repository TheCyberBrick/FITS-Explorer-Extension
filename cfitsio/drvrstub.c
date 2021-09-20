#include "fitsio2.h"
#include "fitsiodrvrstub.h"

stubdriverimpl _stubimpl;

void fits_set_stub_driver_impl(
	void *instance,
	int(*open)(void *instance, char *filename, int rwmode, int *driverhandle),
	int(*read)(void *instance, int drivehandle, void *buffer, long nbytes),
	int(*size)(void *instance, int handle, LONGLONG *filesize),
	int(*seek)(void *instance, int handle, LONGLONG offset),
	int(*create)(void *instance, char *filename, int *handle),
	int(*write)(void *instance, int hdl, void *buffer, long nbytes),
	int(*flush)(void *instance, int handle),
	int(*close)(void *instance, int drivehandle))
{
	stubdriverimpl impl;
	impl.instance = instance;
	impl.open = open;
	impl.read = read;
	impl.size = size;
	impl.seek = seek;
	impl.create = create;
	impl.write = write;
	impl.flush = flush;
	impl.close = close;
	impl.close = close;
	fits_set_stub_driver_impl_struct(impl);
}

void CFITS_API fits_set_stub_driver_impl_struct(stubdriverimpl impl)
{
	_stubimpl = impl;
}

stubdriverimpl fits_get_stub_driver_impl()
{
	return _stubimpl;
}

/****  driver routines for stub//: driver  ********/

/*--------------------------------------------------------------------------*/
int stub_open(char *filename, int rwmode, int *handle)
{
	if (_stubimpl.open)
	{
		return (*_stubimpl.open)(_stubimpl.instance, filename, rwmode, handle);
	}
	ffpmsg("no registered driver stub");
	return 1;
}
/*--------------------------------------------------------------------------*/
int stub_create(char *filename, int *handle)
{
	if (_stubimpl.create)
	{
		return (*_stubimpl.create)(_stubimpl.instance, filename, handle);
	}
	ffpmsg("no registered driver stub");
	return 1;
}
/*--------------------------------------------------------------------------*/
int stub_size(int handle, LONGLONG *filesize)
{
	if (_stubimpl.size)
	{
		return (*_stubimpl.size)(_stubimpl.instance, handle, filesize);
	}
	ffpmsg("no registered driver stub");
	return 1;
}
/*--------------------------------------------------------------------------*/
int stub_close(int handle)
{
	if (_stubimpl.close)
	{
		return (*_stubimpl.close)(_stubimpl.instance, handle);
	}
	ffpmsg("no registered driver stub");
	return 1;
}
/*--------------------------------------------------------------------------*/
int stub_flush(int handle)
{
	if (_stubimpl.flush)
	{
		return (*_stubimpl.flush)(_stubimpl.instance, handle);
	}
	ffpmsg("no registered driver stub");
	return 1;
}
/*--------------------------------------------------------------------------*/
int stub_seek(int handle, LONGLONG offset)
{
	if (_stubimpl.seek)
	{
		return (*_stubimpl.seek)(_stubimpl.instance, handle, offset);
	}
	ffpmsg("no registered driver stub");
	return 1;
}
/*--------------------------------------------------------------------------*/
int stub_read(int hdl, void *buffer, long nbytes)
{
	if (_stubimpl.read)
	{
		return (*_stubimpl.read)(_stubimpl.instance, hdl, buffer, nbytes);
	}
	ffpmsg("no registered driver stub");
	return 1;
}
/*--------------------------------------------------------------------------*/
int stub_write(int hdl, void *buffer, long nbytes)
{
	if (_stubimpl.write)
	{
		return (*_stubimpl.write)(_stubimpl.instance, hdl, buffer, nbytes);
	}
	ffpmsg("no registered driver stub");
	return 1;
}
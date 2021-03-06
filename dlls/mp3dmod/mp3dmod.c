/*
 * MP3 decoder DMO
 *
 * Copyright 2018 Zebediah Figura
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <stdio.h>
#include <mpg123.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "mmreg.h"
#define COBJMACROS
#include "objbase.h"
#include "dmo.h"
#include "rpcproxy.h"
#include "wmcodecdsp.h"
#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(mp3dmod);

static HINSTANCE mp3dmod_instance;

struct mp3_decoder {
    IMediaObject IMediaObject_iface;
    LONG ref;
    mpg123_handle *mh;
    DMO_MEDIA_TYPE outtype;
    IMediaBuffer *buffer;
    REFERENCE_TIME timestamp;
};

static inline struct mp3_decoder *impl_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct mp3_decoder, IMediaObject_iface);
}

static HRESULT WINAPI MediaObject_QueryInterface(IMediaObject *iface, REFIID iid, void **ppv)
{
    struct mp3_decoder *This = impl_from_IMediaObject(iface);

    TRACE("(%p)->(%s, %p)\n", This, debugstr_guid(iid), ppv);

    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IMediaObject))
        *ppv = &This->IMediaObject_iface;
    else
    {
        FIXME("no interface for %s\n", debugstr_guid(iid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IMediaObject_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI MediaObject_AddRef(IMediaObject *iface)
{
    struct mp3_decoder *This = impl_from_IMediaObject(iface);
    ULONG refcount = InterlockedIncrement(&This->ref);

    TRACE("(%p) AddRef from %d\n", This, refcount - 1);

    return refcount;
}

static ULONG WINAPI MediaObject_Release(IMediaObject *iface)
{
    struct mp3_decoder *This = impl_from_IMediaObject(iface);
    ULONG refcount = InterlockedDecrement(&This->ref);

    TRACE("(%p) Release from %d\n", This, refcount + 1);

    if (!refcount)
    {
        mpg123_delete(This->mh);
        heap_free(This);
    }
    return refcount;
}

static HRESULT WINAPI MediaObject_GetStreamCount(IMediaObject *iface, DWORD *input, DWORD *output)
{
    FIXME("(%p)->(%p, %p) stub!\n", iface, input, output);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("(%p)->(%d, %p) stub!\n", iface, index, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("(%p)->(%d, %p) stub!\n", iface, index, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_GetInputType(IMediaObject *iface, DWORD index, DWORD type_index, DMO_MEDIA_TYPE *type)
{
    FIXME("(%p)->(%d, %d, %p) stub!\n", iface, index, type_index, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_GetOutputType(IMediaObject *iface, DWORD index, DWORD type_index, DMO_MEDIA_TYPE *type)
{
    FIXME("(%p)->(%d, %d, %p) stub!\n", iface, index, type_index, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_SetInputType(IMediaObject *iface, DWORD index, const DMO_MEDIA_TYPE *type, DWORD flags)
{
    FIXME("(%p)->(%d, %p, %#x) stub!\n", iface, index, type, flags);

    return S_OK;
}

static HRESULT WINAPI MediaObject_SetOutputType(IMediaObject *iface, DWORD index, const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct mp3_decoder *This = impl_from_IMediaObject(iface);
    WAVEFORMATEX *format;
    long enc;
    int err;

    TRACE("(%p)->(%d, %p, %#x)\n", iface, index, type, flags);

    if (flags & DMO_SET_TYPEF_CLEAR)
    {
        MoFreeMediaType(&This->outtype);
        return S_OK;
    }

    format = (WAVEFORMATEX *)type->pbFormat;

    if (format->wBitsPerSample == 8)
        enc = MPG123_ENC_UNSIGNED_8;
    else if (format->wBitsPerSample == 16)
        enc = MPG123_ENC_SIGNED_16;
    else
    {
        ERR("Cannot decode to bit depth %u.\n", format->wBitsPerSample);
        return DMO_E_TYPE_NOT_ACCEPTED;
    }

    if (!(flags & DMO_SET_TYPEF_TEST_ONLY))
    {
        err = mpg123_format(This->mh, format->nSamplesPerSec, format->nChannels, enc);
        if (err != MPG123_OK)
        {
            ERR("Failed to set format: %u channels, %u samples/sec, %u bits/sample.\n",
                format->nChannels, format->nSamplesPerSec, format->wBitsPerSample);
            return DMO_E_TYPE_NOT_ACCEPTED;
        }
        MoCopyMediaType(&This->outtype, type);
    }

    return S_OK;
}

static HRESULT WINAPI MediaObject_GetInputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    FIXME("(%p)->(%d, %p) stub!\n", iface, index, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_GetOutputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    FIXME("(%p)->(%d, %p) stub!\n", iface, index, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_GetInputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *max_lookahead, DWORD *alignment)
{
    FIXME("(%p)->(%d, %p, %p, %p) stub!\n", iface, index, size, max_lookahead, alignment);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    FIXME("(%p)->(%d, %p, %p) stub!\n", iface, index, size, alignment);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_GetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME *latency)
{
    FIXME("(%p)->(%d, %p) stub!\n", iface, index, latency);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_SetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME latency)
{
    FIXME("(%p)->(%d, %s) stub!\n", iface, index, wine_dbgstr_longlong(latency));

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_Flush(IMediaObject *iface)
{
    FIXME("(%p)->() stub!\n", iface);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_Discontinuity(IMediaObject *iface, DWORD index)
{
    FIXME("(%p)->(%d) stub!\n", iface, index);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_AllocateStreamingResources(IMediaObject *iface)
{
    FIXME("(%p)->() stub!\n", iface);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_FreeStreamingResources(IMediaObject *iface)
{
    FIXME("(%p)->() stub!\n", iface);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_GetInputStatus(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("(%p)->(%d, %p) stub!\n", iface, index, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI MediaObject_ProcessInput(IMediaObject *iface, DWORD index,
    IMediaBuffer *buffer, DWORD flags, REFERENCE_TIME timestamp, REFERENCE_TIME timelength)
{
    struct mp3_decoder *This = impl_from_IMediaObject(iface);
    HRESULT hr;
    BYTE *data;
    DWORD len;
    int err;

    TRACE("(%p)->(%d, %p, %#x, %s, %s)\n", iface, index, buffer, flags,
          wine_dbgstr_longlong(timestamp), wine_dbgstr_longlong(timelength));

    if (This->buffer)
    {
        ERR("Already have a buffer.\n");
        return DMO_E_NOTACCEPTING;
    }

    IMediaBuffer_AddRef(buffer);
    This->buffer = buffer;

    hr = IMediaBuffer_GetBufferAndLength(buffer, &data, &len);
    if (FAILED(hr))
        return hr;

    err = mpg123_feed(This->mh, data, len);
    if (err != MPG123_OK)
    {
        ERR("mpg123_feed() failed: %s\n", mpg123_strerror(This->mh));
        return E_FAIL;
    }

    return S_OK;
}

static DWORD get_framesize(DMO_MEDIA_TYPE *type)
{
    WAVEFORMATEX *format = (WAVEFORMATEX *)type->pbFormat;
    return 1152 * format->nBlockAlign;
}

static REFERENCE_TIME get_frametime(DMO_MEDIA_TYPE *type)
{
    WAVEFORMATEX *format = (WAVEFORMATEX *)type->pbFormat;
    return (REFERENCE_TIME) 10000000 * 1152 / format->nSamplesPerSec;
}

static HRESULT WINAPI MediaObject_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count, DMO_OUTPUT_DATA_BUFFER *buffers, DWORD *status)
{
    struct mp3_decoder *This = impl_from_IMediaObject(iface);
    REFERENCE_TIME time = 0, frametime;
    DWORD len, maxlen, framesize;
    int got_data = 0;
    size_t written;
    HRESULT hr;
    BYTE *data;
    int err;

    TRACE("(%p)->(%#x, %d, %p, %p)\n", iface, flags, count, buffers, status);

    if (count > 1)
        FIXME("Multiple buffers not handled.\n");

    buffers[0].dwStatus = 0;

    if (!This->buffer)
        return S_FALSE;

    buffers[0].dwStatus |= DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT;

    hr = IMediaBuffer_GetBufferAndLength(buffers[0].pBuffer, &data, &len);
    if (FAILED(hr)) return hr;

    hr = IMediaBuffer_GetMaxLength(buffers[0].pBuffer, &maxlen);
    if (FAILED(hr)) return hr;

    framesize = get_framesize(&This->outtype);
    frametime = get_frametime(&This->outtype);

    while (1)
    {
        if (maxlen - len < framesize)
        {
            buffers[0].dwStatus |= DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE;
            break;
        }

        while ((err = mpg123_read(This->mh, data + len, framesize, &written)) == MPG123_NEW_FORMAT);
        if (err == MPG123_NEED_MORE)
        {
            IMediaBuffer_Release(This->buffer);
            This->buffer = NULL;
            break;
        }
        else if (err == MPG123_ERR)
            ERR("mpg123_read() failed: %s\n", mpg123_strerror(This->mh));
        else if (err != MPG123_OK)
            ERR("mpg123_read() returned %d\n", err);
        if (written < framesize)
            ERR("short write: %zd/%u\n", written, framesize);

        got_data = 1;

        len += framesize;
        hr = IMediaBuffer_SetLength(buffers[0].pBuffer, len);
        if (FAILED(hr)) return hr;

        time += frametime;
    }

    if (got_data)
    {
        buffers[0].dwStatus |= (DMO_OUTPUT_DATA_BUFFERF_TIME | DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH);
        buffers[0].rtTimelength = time;
        buffers[0].rtTimestamp = This->timestamp;
        This->timestamp += time;
        return S_OK;
    }
    return S_FALSE;
}

static HRESULT WINAPI MediaObject_Lock(IMediaObject *iface, LONG lock)
{
    FIXME("(%p)->(%d) stub!\n", iface, lock);

    return E_NOTIMPL;
}

static const IMediaObjectVtbl IMediaObject_vtbl = {
    MediaObject_QueryInterface,
    MediaObject_AddRef,
    MediaObject_Release,
    MediaObject_GetStreamCount,
    MediaObject_GetInputStreamInfo,
    MediaObject_GetOutputStreamInfo,
    MediaObject_GetInputType,
    MediaObject_GetOutputType,
    MediaObject_SetInputType,
    MediaObject_SetOutputType,
    MediaObject_GetInputCurrentType,
    MediaObject_GetOutputCurrentType,
    MediaObject_GetInputSizeInfo,
    MediaObject_GetOutputSizeInfo,
    MediaObject_GetInputMaxLatency,
    MediaObject_SetInputMaxLatency,
    MediaObject_Flush,
    MediaObject_Discontinuity,
    MediaObject_AllocateStreamingResources,
    MediaObject_FreeStreamingResources,
    MediaObject_GetInputStatus,
    MediaObject_ProcessInput,
    MediaObject_ProcessOutput,
    MediaObject_Lock,
};

static HRESULT create_mp3_decoder(REFIID iid, void **obj)
{
    struct mp3_decoder *This;
    int err;

    if (!(This = heap_alloc_zero(sizeof(*This))))
        return E_OUTOFMEMORY;

    This->IMediaObject_iface.lpVtbl = &IMediaObject_vtbl;
    This->ref = 0;

    mpg123_init();
    This->mh = mpg123_new(NULL, &err);
    mpg123_open_feed(This->mh);
    mpg123_format_none(This->mh);

    return IMediaObject_QueryInterface(&This->IMediaObject_iface, iid, obj);
}

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFIID iid, void **obj)
{
    TRACE("(%p, %s, %p)\n", iface, debugstr_guid(iid), obj);

    if (IsEqualGUID(&IID_IUnknown, iid) ||
        IsEqualGUID(&IID_IClassFactory, iid))
    {
        IClassFactory_AddRef(iface);
        *obj = iface;
        return S_OK;
    }

    *obj = NULL;
    WARN("no interface for %s\n", debugstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
    return 2;
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
    return 1;
}

static HRESULT WINAPI ClassFactory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID iid, void **obj)
{
    TRACE("(%p, %s, %p)\n", outer, debugstr_guid(iid), obj);

    if (outer)
    {
        *obj = NULL;
        return CLASS_E_NOAGGREGATION;
    }

    return create_mp3_decoder(iid, obj);
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL lock)
{
    FIXME("(%d) stub\n", lock);
    return S_OK;
}

static const IClassFactoryVtbl classfactory_vtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    ClassFactory_CreateInstance,
    ClassFactory_LockServer
};

static IClassFactory mp3_decoder_cf = { &classfactory_vtbl };

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    TRACE("%p, %d, %p\n", instance, reason, reserved);
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        mp3dmod_instance = instance;
        break;
    }
    return TRUE;
}

/*************************************************************************
 *              DllGetClassObject (DSDMO.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **obj)
{
    TRACE("%s, %s, %p\n", debugstr_guid(clsid), debugstr_guid(iid), obj);

    if (IsEqualGUID(clsid, &CLSID_CMP3DecMediaObject))
        return IClassFactory_QueryInterface(&mp3_decoder_cf, iid, obj);

    FIXME("class %s not available\n", debugstr_guid(clsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/******************************************************************
 *              DllCanUnloadNow (DSDMO.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

/***********************************************************************
 *              DllRegisterServer (DSDMO.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources( mp3dmod_instance );
}

/***********************************************************************
 *              DllUnregisterServer (DSDMO.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( mp3dmod_instance );
}

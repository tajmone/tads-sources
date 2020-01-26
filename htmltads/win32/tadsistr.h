/* $Header$ */

/* Copyright (c) 2008 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadsistr.h - TADS IStream implementation
Function
  Simple implementation of IStream for TADS embedded resources.  This
  can be used to pass embedded resource streams to COM interfaces.
Notes

Modified
  11/20/08 MJRoberts  - Creation
*/

#ifndef TADSISTR_H
#define TADSISTR_H

#include <Windows.h>

/*
 *   Stdio file stream reader.  This is a read-only IStream for reading data
 *   from a stream embedded in a larger file, such as a TADS resource file.
 */
class CTadsStdioIStreamReader: public IStream
{
public:
    /* create given a filename */
    CTadsStdioIStreamReader(const char *fname, long start_ofs, long len)
    {
        /* start with one ref on behalf of the caller */
        refs_ = 1;

        /* open the file */
        fp_ = fopen(fname, "rb");

        /* if the length is given as -1, use the whole file size */
        if (len == -1 && fp_ != 0)
        {
            fseek(fp_, 0, SEEK_END);
            len = ftell(fp_) - start_ofs;
        }

        /* if we got a file, seek to the start location */
        if (fp_ != 0)
            fseek(fp_, start_ofs, SEEK_SET);

        /* remember the boundaries of the file */
        start_ofs_ = start_ofs;
        len_ = len;
    }

    /*
     *   create from an existing handle - we'll copy the handle to allow for
     *   safe concurrent access to the original
     */
    CTadsStdioIStreamReader(FILE *fp, long start_ofs, long len)
    {
        /* one ref on behalf of the caller */
        refs_ = 1;

        /* duplicate the file handle */
        fp_ = _fdopen(_dup(_fileno(fp)), "rb");

        /* seek to the start offset */
        if (fp_ != 0)
            fseek(fp_, start_ofs, SEEK_SET);

        /* remember the boundaries */
        start_ofs_ = start_ofs;
        len_ = len;
    }

    /* destruction */
    virtual ~CTadsStdioIStreamReader()
    {
        if (fp_ != 0)
            fclose(fp_);
    }

    /* COM reference counting */
    ULONG STDMETHODCALLTYPE AddRef() { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release()
    {
        LONG ret = --refs_;
        if (ret == 0)
            delete this;
        return ret;
    }

    /* COM interface queries */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **obj)
    {
        if (iid == IID_IStream)
        {
            *obj = this;
            return S_OK;
        }
        if (iid == IID_ISequentialStream)
        {
            *obj = this;
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    /* read from the file */
    HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead)
    {
        /* read the data */
        int actual = fread(pv, 1, cb, fp_);

        /* tell the caller how much we read, if desired */
        if (pcbRead != 0)
            *pcbRead = actual;

        /* success */
        return S_OK;
    }

    /* write - we're a read-only stream, so we don't allow this */
    HRESULT STDMETHODCALLTYPE Write(
        const void *pv, ULONG cb, ULONG *pcbWritten)
    {
        /* tell the caller zero bytes were written */
        if (pcbWritten != 0)
            *pcbWritten = 0;

        /* indicate that writing is not allowed */
        return STG_E_ACCESSDENIED;
    }

    /* make a copy of the stream */
    HRESULT STDMETHODCALLTYPE Clone(IStream **str)
    {
        *str = new CTadsStdioIStreamReader(fp_, start_ofs_, len_);
        return S_OK;
    }

    /* commit - we're read-only, so this does nothing */
    HRESULT STDMETHODCALLTYPE Commit(DWORD flags) { return S_OK; }

    /* copy data to another stream */
    HRESULT STDMETHODCALLTYPE CopyTo(
        IStream *dst, ULARGE_INTEGER bytes,
        ULARGE_INTEGER *actual_read, ULARGE_INTEGER *actual_written)
    {
        /* presume success */
        HRESULT res = S_OK;

        /* clear out the byte counters */
        if (actual_read != 0)
            actual_read->QuadPart = 0;
        if (actual_written != 0)
            actual_written->QuadPart = 0;

        /* keep going until we satisfy the request */
        while (bytes.QuadPart != 0)
        {
            /*
             *   set up a local buffer, and read one buffer-full or the
             *   remainder of the request, whichever is smaller
             */
            char buf[2048];
            size_t cur = sizeof(buf);
            if (cur > bytes.QuadPart)
                cur = (size_t)bytes.LowPart;

            /* read the chunk */
            cur = fread(buf, 1, cur, fp_);

            /* if we got zero bytes, we're at EOF, so we're done */
            if (cur == 0)
                break;

            /* update the read counter, if provided */
            if (actual_read != 0)
                actual_read->QuadPart += cur;

            /* deduct this chunk from the remaining request */
            bytes.QuadPart -= cur;

            /* write it out to the destination stream */
            ULONG cur_actual;
            res = dst->Write(buf, cur, &cur_actual);

            /* update the write counter, if provided */
            if (actual_written != 0)
                actual_written->QuadPart += cur_actual;

            /* if the write failed, stop and return the error to our caller */
            if (FAILED(res))
                return res;
        }

        /* success */
        return S_OK;
    }

    /* locks - we don't implement locking  */
    HRESULT STDMETHODCALLTYPE LockRegion(
        ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE UnlockRegion(
        ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
    {
        return E_NOTIMPL;
    }

    /* revert - we're read-only, so this does nothing */
    HRESULT STDMETHODCALLTYPE Revert() { return S_OK; }

    /* seek within the file */
    HRESULT STDMETHODCALLTYPE Seek(
        LARGE_INTEGER pos, DWORD origin, ULARGE_INTEGER *newpos)
    {
        /* figure the absolute seek position within the stream */
        switch (origin)
        {
        case STREAM_SEEK_SET:
            /* seeking from the beginning of the stream */
            break;

        case STREAM_SEEK_END:
            /*
             *   seeking from the end of the stream - get offset from the end
             *   of the stream by adding the total stream length
             */
            pos.QuadPart += len_;
            break;

        case STREAM_SEEK_CUR:
            /* seeking from current position - figure based on current pos */
            pos.QuadPart += ftell(fp_) - start_ofs_;
            break;

        default:
            return E_INVALIDARG;
        }

        /* limit the seek to the stream boundaries */
        if (pos.QuadPart < 0)
            pos.QuadPart = 0;
        if (pos.QuadPart > len_)
            pos.QuadPart = len_;

        /* do the seek within the file, adjusting by the stream start point */
        if (fseek(fp_, pos.LowPart + start_ofs_, SEEK_SET) == -1)
            return E_FAIL;

        /* if the caller wants the new position, get it */
        if (newpos != 0)
            newpos->QuadPart = ftell(fp_) - start_ofs_;

        /* success */
        return S_OK;
    }

    /* set file size - we're read-only, so this isn't allowed */
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER)
    {
        return STG_E_ACCESSDENIED;
    }

    /* get status - we don't implement this */
    HRESULT STDMETHODCALLTYPE Stat(STATSTG *pstat, DWORD flag)
    {
        return E_NOTIMPL;
    }


protected:
    /* reference count */
    ULONG refs_;

    /* file handle and stream boundaries within the file */
    FILE *fp_;
    long start_ofs_;
    long len_;
};

#endif /* TADSISTR_H */

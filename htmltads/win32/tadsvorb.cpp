/*
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsvorb.cpp - TADS Ogg-Vorbis decoder
Function
  Defines a compressed audio format decoder and playback engine for the
  Ogg Vorbis format.
Notes

Modified
  04/26/02 MJRoberts  - creation
*/

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <io.h>

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

#include "tadscsnd.h"
#include "tadsvorb.h"

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER -1
#endif

/* ------------------------------------------------------------------------ */
/*
 *   construction
 */
CVorbisW32::CVorbisW32(
    const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
    struct IDirectSound *ds, class CTadsAudioControl *ctl,
    HWND hwnd, void (*done_func)(void *, int), void *done_func_ctx)
    : CTadsCompressedAudio(fname, file_start_ofs, file_size, ds, ctl,
                           hwnd, done_func, done_func_ctx)
{
    /* not stopping yet */
    stop_flag_ = FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Callbacks for data source operations.  We want to use a Windows native
 *   file handle rather than a stdio FILE object, so we implement our own
 *   set of data source callbacks.
 */

/* data source - this is the context for our callbacks */
struct datasource_t
{
    datasource_t(HANDLE hf, unsigned long siz)
    {
        /* remember the file handle */
        hfile = hf;

        /* remember the starting seek location */
        stream_base = SetFilePointer(hf, 0, 0, FILE_CURRENT);

        /* remember the file size */
        stream_size = siz;
    }

    /* the Windows file handle */
    HANDLE hfile;

    /*
     *   The seek location of the start of our data stream in the file.  If
     *   this is a .ogg file, this is simply 0, since the file contains only
     *   the Ogg data stream.  If we're reading an Ogg resource embedded in a
     *   *.gam/.t3 file or in a resource file, this is the start of the
     *   embedded .ogg file within the game/resource file.
     */
    unsigned long stream_base;

    /*
     *   The data stream size in bytes.  For a stand-alone .ogg file, this is
     *   simply the same as the size of the entire file.  For an embedded
     *   resource, it's the size of the resource stream within the larger
     *   file.
     */
    unsigned long stream_size;
};

/* read from the file */
static size_t cb_read(void *buf, size_t siz, size_t cnt, void *fp)
{
    datasource_t *ds = (datasource_t *)fp;
    DWORD req;
    DWORD actual;

    /* calculate the size in bytes */
    req = siz * cnt;

    /* limit the request to the remaining file size */
    DWORD cur_ofs = SetFilePointer(ds->hfile, 0, 0, FILE_CURRENT)
                    - ds->stream_base;
    if (cur_ofs + req > ds->stream_size)
        req = ds->stream_size - cur_ofs;

    /* read the data */
    if (ReadFile(ds->hfile, buf, req, &actual, 0))
    {
        /* success - return the number of bytes actually read */
        return actual;
    }
    else
    {
        /* failure - return zero */
        return 0;
    }
}

/* seek */
static int cb_seek(void *fp, ogg_int64_t offset, int whence)
{
    datasource_t *ds = (datasource_t *)fp;
    DWORD mode;
    LONG result;

    /* translate the stdio-style mode to the Windows mode */
    switch(whence)
    {
    case SEEK_SET:
        /* relative to start of file */
        mode = FILE_BEGIN;

        /* adjust for the stream base, in case we're an embedded resource */
        offset += ds->stream_base;
        break;

    case SEEK_CUR:
        /* relative to current position */
        mode = FILE_CURRENT;
        break;

    case SEEK_END:
        /*
         *   seek to the end of the data stream, NOT the end of the overall
         *   file - do this by seeking to the stream base plus the data
         *   stream size
         */
        mode = FILE_BEGIN;
        offset += (ds->stream_base + ds->stream_size);
        break;

    default:
        /* failure */
        return -1;
    }

    /* decompose the destination into 32-bit values */
    LONG pos_lo = (LONG)offset;
    LONG pos_hi = (LONG)(offset >> 32);

    /* set the file position */
    result = SetFilePointer(
        ds->hfile, pos_lo, pos_hi == 0 ? 0 : &pos_hi, mode);

    /* if there's no high part, decoding the result is easy */
    if (pos_hi == 0)
        return (result == INVALID_SET_FILE_POINTER ? -1 : 0);

    /* if there's a high part, we have to check GetLastError */
    return (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR
            ? -1 : 0);
}

/* close */
static int cb_close(void *fp)
{
    /*
     *   the file handle is managed externally to the decoder, so we don't
     *   need to do anything here
     */
    return 0;
}

/* get current position */
static long cb_tell(void *fp)
{
    datasource_t *ds = (datasource_t *)fp;

    /*
     *   get the current file position by seeking to the current position,
     *   and adjust it so that it's relative to the stream base address
     */
    return SetFilePointer(ds->hfile, 0, 0, FILE_CURRENT) - ds->stream_base;
}


/* ------------------------------------------------------------------------ */
/*
 *   Get the track length
 */
long CVorbisW32::get_track_len_ms()
{
    OggVorbis_File vf;
    ov_callbacks cb =
    {
        &cb_read,
        &cb_seek,
        &cb_close,
        &cb_tell
    };
    HANDLE hfile;

    /*
     *   open the file - use a separate handle so that we don't interfere
     *   with playback in another thread
     */
    hfile = CreateFile(fname_.get(), GENERIC_READ, FILE_SHARE_READ,
                       0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    /* seek to the start of the sound, in case it's an embedded resource */
    SetFilePointer(hfile, in_file_start_, 0, FILE_BEGIN);

    /* set up our vorbis data source on the file */
    datasource_t ds(hfile, in_file_size_);

    /* final layer: open the vorbisfile descriptor on the stdio FILE */
    if (ov_open_callbacks(&ds, &vf, 0, 0, cb) < 0)
    {
        CloseHandle(hfile);
        return 0;
    }

    /*
     *   figure the time for the whole file - the OV routine returns the time
     *   in seconds as a double, and we want milliseconds as a long, so
     *   multiply by 1000 and convert types
     */
    double s = ov_time_total(&vf, -1);
    long ms = (long)(s * 1000.0);

    /* cleanup */
    ov_clear(&vf);
    CloseHandle(hfile);

    /* return the time */
    return ms;
}

/* ------------------------------------------------------------------------ */
/*
 *   Decode a file
 */
void CVorbisW32::do_decoding(HANDLE hfile, DWORD file_size)
{
    OggVorbis_File vf;
    vorbis_info *vi;
    int eof = FALSE;
    ov_callbacks cb =
    {
        &cb_read,
        &cb_seek,
        &cb_close,
        &cb_tell
    };
    int active_sect;
    datasource_t ds(hfile, file_size);

    /* open the vorbisfile descriptor on our file handle */
    if (ov_open_callbacks(&ds, &vf, 0, 0, cb) < 0)
    {
        // $$$"Input does not appear to be an Ogg bitstream."
        return;
    }

    /* get information on the current logical bitstream */
    vi = ov_info(&vf, -1);

    /* open our playback buffer */
    open_playback_buffer(vi->rate, 16, vi->channels);

    /* start with section 0 */
    // $$$ this might not be right
    active_sect = 0;

    /* decode until done */
    for (eof = FALSE ; !eof && !stop_flag_ ; )
    {
        long len;
        int cur_sect;

        /*
         *   read the next chunk - read 16-bit signed samples in
         *   little-endian mode
         */
        len = ov_read(&vf, pcmbuf, sizeof(pcmbuf), 0, 2, 1, &cur_sect);

        /*
         *   if we changed to a new section, we might have to change to a
         *   new sampling rate
         */
        if (cur_sect != active_sect)
        {
            vorbis_info *new_vi;

            /* get the new section's information */
            new_vi = ov_info(&vf, cur_sect);

            /*
             *   if the sampling rate or channel count has changed, close
             *   and re-open the audio buffer with the new specifications
             */
            if (new_vi->rate != vi->rate
                || new_vi->channels != vi->channels)
            {
                /* close the playback buffer */
                close_playback_buffer();

                /* open it up again with the new specs */
                open_playback_buffer(new_vi->rate, 16, new_vi->channels);
            }

            /* remember this as the new active section */
            active_sect = cur_sect;
            vi = new_vi;
        }

        /* if we didn't get any data, we've reached the end of the file */
        if (len == 0)
        {
            /* flag that we're at EOF */
            eof = TRUE;
        }
        else if (len < 0)
        {
            /* error in the stream */
            // $$$ might want to report it
        }
        else
        {
            /* write the data to our playback buffer */
            write_playback_buffer(pcmbuf, len);
        }
    }

    /* cleanup */
    ov_clear(&vf);
}


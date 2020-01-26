#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadswav.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadswav.cpp - TADS WAV (waveform audio) file support
Function

Notes

Modified
  01/19/98 MJRoberts  - Creation
*/

#include <Windows.h>
#include <dsound.h>

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <process.h>

/* include TADS OS layer for I/O functions */
#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSCSND_H
#include "tadscsnd.h"
#endif
#ifndef TADSWAV_H
#include "tadswav.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Wave file decoder
 */

/*
 *   construction
 */
CWavW32::CWavW32(
    const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
    struct IDirectSound *ds, class CTadsAudioControl *ctl,
    HWND hwnd, void (*done_func)(void *, int), void *done_func_ctx)
    : CTadsCompressedAudio(fname, file_start_ofs, file_size, ds, ctl,
                           hwnd, done_func, done_func_ctx)
{
}

/*
 *   Get the track length
 */
long CWavW32::get_track_len_ms()
{
    /* presume we won't be able to figure out the play time */
    long ms = 0;

    /* open a separate handle to the file, to avoid thread interference */
    HANDLE hfile = CreateFile(fname_.get(), GENERIC_READ, FILE_SHARE_READ,
                              0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hfile == 0)
        return 0;

    /* seek to the start of the sound, in case it's an embedded resource */
    SetFilePointer(hfile, in_file_start_, 0, FILE_BEGIN);

    /* read the header */
    tads_wav_hdr_info hdr;
    if (!read_header(hfile, &hdr) && hdr.wavefmt_ != 0)
    {
        /*
         *   The play time is the data length (in bytes) divided by the
         *   average data rate (in bytes per second).  If the average data
         *   rate in the header is zero, and the format is PCM, we can
         *   calculate the average data rate as the samples per second times
         *   the block alignment; for other formats we can't compute this,
         *   since the data could be compressed.
         */
        DWORD avg_rate = hdr.wavefmt_->nAvgBytesPerSec;
        if (avg_rate == 0 && hdr.wavefmt_->wFormatTag == WAVE_FORMAT_PCM)
        {
            /* it's PCM, so calculate the average based on the sample rate */
            avg_rate = hdr.wavefmt_->nSamplesPerSec
                       * hdr.wavefmt_->nBlockAlign;
        }

        /* if we have an average data rate, figure the play time */
        if (avg_rate != 0)
        {
            /* figure the time in seconds as a double */
            double s = (double)hdr.data_len_ / (double)avg_rate;

            /* convert to milliseconds and truncate to integer */
            ms = (long)(s * 1000.0);
        }
    }

    /* done with the file */
    CloseHandle(hfile);

    /* return the time in millisecond */
    return ms;
}

/*
 *   Decode the file
 */
void CWavW32::do_decoding(HANDLE hfile, DWORD file_size)
{
    int repeats_done;

    /* not stopping yet */
    stop_flag_ = FALSE;

    /*
     *   read the header - if that fails, or we can't find the format
     *   structure, we can't read the file
     */
    if (read_header(hfile, &hdr_)
        || hdr_.wavefmt_ == 0
        || !hdr_.found_data_
        || !hdr_.found_header_
        || hdr_.wavefmt_->wBitsPerSample == 0)
    {
        // $$$ should probably report the error

        /* nothing more to do */
        goto done;
    }

    /* open the playback buffer */
    open_playback_buffer(hdr_.wavefmt_->nSamplesPerSec,
                         hdr_.wavefmt_->wBitsPerSample,
                         hdr_.wavefmt_->nChannels);

    /* start at the beginning of the file */
    data_read_ofs_ = 0;

    /* read data and play it back until we run out of data */
    for (repeats_done = 0 ; !stop_flag_ && repeats_done < 1 ; )
    {
        unsigned long actual;

        /* read bytes into our buffer */
        read_data(hfile, pcmbuf_, sizeof(pcmbuf_), &actual,
                  1, &repeats_done);

        /* if we didn't get any data, we've reached the end */
        if (actual == 0)
            break;

        /* write the data to our playback buffer */
        write_playback_buffer(pcmbuf_, actual);
    }

done: ;
}

/*
 *   Read a WAV file header.  Before calling this, seek to the point in
 *   the file where the WAV data begins.  This parses the file header and
 *   sets up the WAVEFORMATEX structure.  Returns zero on success,
 *   non-zero on failure.
 */
int CWavW32::read_header(HANDLE hfile, tads_wav_hdr_info *hdr)
{
    unsigned char buf[12];
    unsigned long chunklen;
    unsigned long fpos, endpos;
    DWORD bytes_read;

    /* read the RIFF header, first chunk length, and first chunk type */
    if (ReadFile(hfile, buf, 12, &bytes_read, 0) == 0 || bytes_read != 12)
        return 1;

    /*
     *   check the header - make sure it's a RIFF file whose first chunk
     *   is WAVE
     */
    if (memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0)
        return 2;

    /* note our current location */
    fpos = SetFilePointer(hfile, 0, 0, FILE_CURRENT);

    /*
     *   Get the length of the WAV chunk - it's a 4-byte little-endian value,
     *   just like our own native unsigned long type (as this is Windows-only
     *   code reading a Windows file format, what a coincidence!).  From the
     *   length, calculate the ending location.
     */
    memcpy(&chunklen, buf + 4, 4);
    endpos = fpos + chunklen - 4;

    /* presume we won't find the subchunks */
    hdr->found_data_ = hdr->found_header_ = FALSE;

    /*
     *   Scan the WAVE chunk for subchunks.  It should have two: one
     *   labelled "fmt " with the WAV header, and one labelled "data" with
     *   the PCM byte stream.  We want to read the "fmt " header and
     *   remember the location of the "data" chunk so that we can stream
     *   it in for playback later.
     */
    while (fpos < endpos)
    {
        unsigned long sublen;

        /* read the next chunk header (type code plus 4-byte length) */
        if (ReadFile(hfile, buf, 8, &bytes_read, 0) == 0 || bytes_read != 8)
            return 0;

        /* get the subchunk length */
        memcpy(&sublen, buf + 4, 4);

        /* note the change in file position */
        fpos += 8;

        /* see what we have */
        if (memcmp(buf, "fmt ", 4) == 0)
        {
            PCMWAVEFORMAT pcmwavefmt;
            WORD extra_len;

            /* this is the header chunk */
            hdr->found_header_ = TRUE;

            /*
             *   make sure it's big enough to be a WAV file - it has to at
             *   least contain a PCMWAVEFORMAT structure
             */
            if (sublen < sizeof(PCMWAVEFORMAT))
                return 0;

            /* read the PCMWAVEFORMAT structure */
            if (ReadFile(hfile, &pcmwavefmt, sizeof(pcmwavefmt),
                         &bytes_read, 0) == 0
                || bytes_read != sizeof(pcmwavefmt))
                return 0;

            /*
             *   Check the format tag.  If it's WAVE_FORMAT_PCM, it means
             *   that the PCMWAVEFORMAT structure we just read is the
             *   whole thing.  Otherwise, read a 16-bit integer which will
             *   tell us how many extra bytes there are in the structure.
             */
            if (pcmwavefmt.wf.wFormatTag == WAVE_FORMAT_PCM)
            {
                /* vanilla PCMWAVEFORMAT structure; nothing extra */
                extra_len = 0;
            }
            else
            {
                /*
                 *   the next 16-bit word in the file is the number of
                 *   extra header bytes
                 */
                if (ReadFile(hfile, &extra_len, sizeof(extra_len),
                             &bytes_read, 0) == 0
                    || bytes_read != sizeof(extra_len))
                    return 0;
            }

            /* allocate space for the header structure */
            hdr->alloc_wavefmt(extra_len);

            /* copy the PCMWAVEFORMAT base structure */
            memcpy(hdr->wavefmt_, &pcmwavefmt, sizeof(pcmwavefmt));

            /* read the extra bytes, if there are any */
            hdr->wavefmt_->cbSize = extra_len;
            if (extra_len != 0
                && (ReadFile(hfile,
                             (((BYTE *)&hdr->wavefmt_->cbSize)
                              + sizeof(hdr->wavefmt_->cbSize)), extra_len,
                             &bytes_read, 0) == 0
                    || bytes_read != extra_len))
                return 9;
        }
        else if (memcmp(buf, "data", 4) == 0)
        {
            /* this is the data chunk - note its location */
            hdr->found_data_ = TRUE;
            hdr->data_fpos_ = fpos;
            hdr->data_len_ = sublen;
        }
        else
        {
            /* ignore other subchunk types */
        }

        /* seek to the start of the next subchunk */
        fpos += ((sublen + 1) & ~1);
        SetFilePointer(hfile, fpos, 0, FILE_BEGIN);
    }

    /* if we didn't find either chunk, it's not a valid file */
    if (!hdr->found_data_ || !hdr->found_header_)
        return 5;

    /* success */
    return 0;
}

/*
 *   Read from the data section
 */
int CWavW32::read_data(HANDLE hfile, char *buf,
                       unsigned long bytes_to_read,
                       unsigned long *bytes_read,
                       int repeat, int *repeats_done)
{
    DWORD actual_read;
    unsigned long org_bytes_to_read = bytes_to_read;
    unsigned long avail;
    unsigned char *org_buf = (unsigned char *)buf;
    unsigned char *cur_buf;

    /* seek to the next read position */
    if (SetFilePointer(hfile, hdr_.data_fpos_ + data_read_ofs_, 0,
                       FILE_BEGIN) == 0xffffffff)
        return 1;

    /*
     *   Figure out how many bytes are available to be read.  We can read
     *   the amount remaining on this iteration, plus the full data size
     *   times the number of additional iterations remaining.  If we're
     *   repeating indefinitely, we can read any amount.
     */
    if (repeat == 0)
    {
        /* repeating forever - we can read as much as they want */
        avail = bytes_to_read;
    }
    else
    {
        /* start off with the amount left on the current iteration */
        avail = hdr_.data_len_ - data_read_ofs_;

        /* add additional iterations, if any are available */
        if (repeat > *repeats_done)
            avail += hdr_.data_len_ * (repeat - *repeats_done - 1);
    }

    /*
     *   Determine if we can satisfy the request, based on how many bytes
     *   we have remaining in the chunk.  If we can't satisfy the request,
     *   reduce the number of bytes to read to the number actually
     *   remaining, and load the bytes into the end of the buffer.  We
     *   load into the end of the buffer because of the way direct sound
     *   notifications work: we can only set up a notification at certain
     *   fixed points, so we need to align the end of the file against one
     *   of these fixed points.  The buffer that was passed in ends at one
     *   of these notification points, so put our data at the end of the
     *   caller's buffer.
     */
    if (bytes_to_read > avail)
    {
        /* shorten the amount to read by the actual amount remaining */
        bytes_to_read = avail;
    }

    /*
     *   Read the file.  First read as much as we have left on the current
     *   iteration, then loop back to the start of the file and read from
     *   the beginning until we satisfy the request, looping back again
     *   and again if necessary.
     */
    for (cur_buf = (unsigned char *)buf, actual_read = 0 ;
         bytes_to_read != 0 ; )
    {
        unsigned long cur_len;
        unsigned long cur_actual;

        /* read up to the amount left on the current iteration */
        cur_len = bytes_to_read;
        if (cur_len > hdr_.data_len_ - data_read_ofs_)
            cur_len = hdr_.data_len_ - data_read_ofs_;

        /* read this block */
        if (ReadFile(hfile, cur_buf, cur_len, &cur_actual, 0) == 0
            || cur_actual != cur_len)
            return 1;

        /* add this read into the total */
        actual_read += cur_actual;

        /* advance the read offset */
        data_read_ofs_ += cur_actual;

        /* advance the buffer pointer */
        cur_buf += cur_len;

        /* decrement the amount left to read */
        bytes_to_read -= cur_len;

        /* if we've exhausted the file, count the completed iteration */
        if (data_read_ofs_ == hdr_.data_len_)
            ++(*repeats_done);

        /*
         *   if there's anything left to read, wrap to the start of the
         *   data -- the only way we will need to read in multiple blocks
         *   in this loop is if we need to loop back to the start of the
         *   file, so do so here
         */
        if (bytes_to_read != 0)
        {
            /* reset to the start of the data stream */
            data_read_ofs_ = 0;
            if (SetFilePointer(hfile, hdr_.data_fpos_, 0, FILE_BEGIN)
                == 0xffffffff)
                return 1;
        }
    }

    /* set the amount we read for the caller's information */
    *bytes_read = actual_read;

    /* success */
    return 0;
}


#if 0
/* ------------------------------------------------------------------------ */
/*
 *   Test section
 */

static void errexit(const char *msg)
{
    printf("error: %s\n", msg);
    exit(1);
}

int main()
{
    osfildef *fp;
    CWavW32 *reader;
    int err;

    /* open a file */
    fp = osfoprb("c:\\win95\\msremind.wav", OSFTBIN);
    if (fp == 0)
        errexit("can't open file");

    /* create a reader */
    reader = new CWavW32();

    /* read the header */
    if ((err = reader->read_header(fp)) != 0)
    {
        char buf[128];
        sprintf(buf, "return code %d reading header", err);
        errexit(buf);
    }

    /* done */
    delete reader;
    osfcls(fp);
    return 0;
}

void oshtml_dbg_printf(const char *, ...)
{
}


#endif

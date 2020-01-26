/* $Header: d:/cvsroot/tads/html/win32/mpegamp/mpegamp_w32.h,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  audio_w32.h - win95 audio driver for AMP 0.7.6 MPEG player
Function

Notes
  Derived from amp 0.7.6 for use in HTML TADS.
Modified
  10/25/98 MJRoberts  - Creation
*/

#ifndef MPEGAMP_W32_H
#define MPEGAMP_W32_H


#ifndef MPEGAMP_H
#include "mpegamp.h"
#endif
#ifndef TADSCSND_H
#include "tadscsnd.h"
#endif


/*
 *   MPEG audio decompressor for Win32
 */
class CMpegAmpW32: public CMpegAmp, public CTadsCompressedAudio
{
    friend class CMpegAmp;
    friend class CMpegTimeParser;

public:
    CMpegAmpW32(
        const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
        struct IDirectSound *ds, class CTadsAudioControl *ctl,
        HWND hwnd, void (*done_func)(void *, int), void *done_func_ctx);
    ~CMpegAmpW32() { }

    /*
     *   TESTING ONLY - Play a file.  This operates synchronously; it doesn't
     *   return until playback finishes.  If 'stop_on_key' is true, we'll
     *   cancel playback if the user presses a key during playback.
     */
    void play(const char *inFileStr, int stop_on_key);

    /* get the track time in milliseconds */
    long get_track_len_ms();

protected:
    /* decode a file */
    void do_decoding(HANDLE hfile, DWORD file_size)
    {
        /* remember the file in the decoder's member variables */
        in_file = hfile;
        file_bytes_avail = file_size;

        /* we're set up now, so decode the file */
        decodeMPEG();
    }

    /*
     *   get/set the decoder 'stop' flag - we use the mpeg decoder's internal
     *   stop flag
     */
    virtual int get_decoder_stopping() { return stop_flag; }
    virtual void set_decoder_stopping(int f) { stop_flag = f; }

    /* set up the output stream */
    virtual void open_output_buffer(int freq, int nch)
        { open_playback_buffer(freq, 16, nch); }
};

/* ------------------------------------------------------------------------ */
/*
 *   MPEG audio play-time parser.  This is basically a hacked version of the
 *   actual decoder class, ditching most of the decoding work but keeping
 *   enough of the format parser to measure the duration of a particular MP3
 *   track.  The duration of a track is the sum of the durations of the
 *   frames, and the durations of a frame can be determined from its bit rate
 *   and size.
 */
class CMpegTimeParser: public CMpegAmp
{
public:
    CMpegTimeParser(const textchar_t *fname,
                    DWORD file_start_ofs, DWORD file_size);
    ~CMpegTimeParser();

    /* parse the file and return the length in milliseconds */
    long get_play_time_ms();

protected:
    /* have we done a decoding pass yet? */
    int did_decode_;

    /* set up the output stream - we don't process output, so do nothing */
    virtual void open_output_buffer(int freq, int nch) { }
};

#endif /* AUDIO_W95_H */


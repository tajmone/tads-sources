/*
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsvorb.h - TADS Ogg-Vorbis decoder
Function
  Defines a compressed audio format decoder and playback engine for
  the Ogg Vorbis format.
Notes

Modified
  04/26/02 MJRoberts  - creation
*/

#ifndef TADSVORB_H
#define TADSVORB_H

#include "tadscsnd.h"

/*
 *   TADS Ogg Vorbis decoder and playback engine
 */
class CVorbisW32: public CTadsCompressedAudio
{
public:
    /* create */
    CVorbisW32(const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
               struct IDirectSound *ds, class CTadsAudioControl *ctl,
               HWND hwnd, void (*done_func)(void *, int), void *done_func_ctx);

    /* get the track time in milliseconds */
    long get_track_len_ms();

    /* decode the file */
    virtual void do_decoding(HANDLE hfile, DWORD file_size);

    /* get/set our 'stop' flag */
    virtual int get_decoder_stopping() { return stop_flag_; }
    void set_decoder_stopping(int f) { stop_flag_ = f; }

private:
    /* file information */
    HANDLE hfile_;
    DWORD file_size_;

    /* decoder stop flag */
    volatile int stop_flag_;

    /* decoding work buffer */
    char pcmbuf[4096];
};

#endif /* TADSVORB_H */

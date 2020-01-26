/* $Header: d:/cvsroot/tads/html/win32/w32snd.h,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32snd.h - tads html win32 sound implementation
Function

Notes

Modified
  01/31/98 MJRoberts  - Creation
*/

#ifndef W32SND_H
#define W32SND_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef TADSSND_H
#include "tadssnd.h"
#endif
#ifdef TADSMIDI_H
#include "tadsmidi.h"
#endif
#ifndef TADSCSND_H
#include "tadscsnd.h"
#endif


/*------------------------------------------------------------------------ */
/*
 *   MIDI Sound implementation
 */
class CHtmlSysSoundMidi_win32: public CHtmlSysSoundMidi, public CTadsSound
{
public:
    /*
     *   CHtmlSysSoundMidi implementation
     */
    CHtmlSysSoundMidi_win32(class CTadsAudioControl *ctl);
    ~CHtmlSysSoundMidi_win32();

    /* play the sound */
    int play_sound(class CHtmlSysWin *win, void (*done_func)(void *, int),
                   void *done_func_ctx, int repeat, const textchar_t *url,
                   int vol, long fade_in, long fade_out, int crossfade_out);

    /* cancel the sound */
    void cancel_sound(class CHtmlSysWin *win, int sync,
                      long fade_out_ms, int fade_in_bg)
        { CTadsSound::cancel_sound(win, sync, fade_out_ms, fade_in_bg); }

    /* add a crossfade */
    void add_crossfade(class CHtmlSysWin *win, long ms)
        { CTadsSound::add_crossfade(win, ms); }

    /* suspend if necessary to make room for another sound */
    int maybe_suspend(class CHtmlSysSound *new_sound);

    /* resume playback of suspended sound */
    void resume();

    /* get/detach the player object */
    virtual class CTadsAudioPlayer *get_player() const;
    virtual void detach_player();

private:
    /* our audio control */
    class CTadsAudioControl *ctl_;

    /* our MIDI player object */
    class CTadsMidiFilePlayer *player_;

    /* our file pointer */
    osfildef *fp_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Base class for win32 digitized audio type implementations
 */
class CHtmlSysSoundDigitized_win32: public CTadsSound
{
public:
    CHtmlSysSoundDigitized_win32();
    virtual ~CHtmlSysSoundDigitized_win32();

    /* play the sound */
    int play_sound(class CHtmlSysWin *win,
                   void (*done_func)(void *, int),
                   void *done_func_ctx, int repeat,
                   const textchar_t *url,
                   int vol, long fade_in, long fade_out, int crossfade_out);

    /* suspend if necessary to make room for another sound */
    int maybe_suspend(class CHtmlSysSound *new_sound);

    /* resume playback of suspended sound */
    void resume();

    /* get/detach the player object */
    virtual class CTadsAudioPlayer *get_player() const { return player_; }
    virtual void detach_player()
    {
        if (player_ != 0)
        {
            player_->Release();
            player_ = 0;
        }
    }

protected:
    /*
     *   create the player - must be overridden per subclass to create the
     *   appropriate player subclass for the audio datatype
     */
    virtual class CTadsCompressedAudio *create_player(
        const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
        IDirectSound *ds, class CTadsAudioControl *ctl,
        HWND hwnd, void (*done_func)(void *, int), void *done_func_ctx) = 0;

    /* our player object */
    class CTadsCompressedAudio *player_;
};

/*------------------------------------------------------------------------ */
/*
 *   WAV Sound implementation
 */
class CHtmlSysSoundWav_win32: public CHtmlSysSoundWav,
    public CHtmlSysSoundDigitized_win32
{
public:
    /*
     *   CHtmlSysSoundWav implementation
     */

    CHtmlSysSoundWav_win32()
        : CHtmlSysSoundWav(), CHtmlSysSoundDigitized_win32()
    {
    }

    /*
     *   Map the CHtmlSysSound interface to the CHtmlSysSoundDigitized_win32
     *   implementation
     */
    int play_sound(class CHtmlSysWin *win,
                   void (*done_func)(void *, int),
                   void *done_func_ctx, int repeat,
                   const textchar_t *url,
                   int vol, long fade_in, long fade_out, int crossfade_out)
    {
        return CHtmlSysSoundDigitized_win32::play_sound(
            win, done_func, done_func_ctx, repeat, url,
            vol, fade_in, fade_out, crossfade_out);
    }

    /* add a crossfade */
    void add_crossfade(class CHtmlSysWin *win, long ms)
        { CHtmlSysSoundDigitized_win32::add_crossfade(win, ms); }

    /* cancel the sound */
    void cancel_sound(
        class CHtmlSysWin *win, int sync, long fade_out_ms, int fade_in_bg)
    {
        CHtmlSysSoundDigitized_win32::cancel_sound(
            win, sync, fade_out_ms, fade_in_bg);
    }

    int maybe_suspend(class CHtmlSysSound *new_sound)
        { return CHtmlSysSoundDigitized_win32::maybe_suspend(new_sound); }

    void resume()
        { CHtmlSysSoundDigitized_win32::resume(); }

private:
    /* create the player object */
    virtual CTadsCompressedAudio *create_player(
        const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
        IDirectSound *ds, class CTadsAudioControl *ctl,
        HWND hwnd, void (*done_func)(void *, int), void *done_func_ctx);
};

/* ------------------------------------------------------------------------ */
/*
 *   MPEG 2.0 audio layer 1/2/3 sound implementation
 */
class CHtmlSysSoundMpeg_win32: public CHtmlSysSoundMpeg,
    public CHtmlSysSoundDigitized_win32
{
public:
    /*
     *   CHtmlSysSoundMpeg implementation
     */
    CHtmlSysSoundMpeg_win32()
        : CHtmlSysSoundMpeg(), CHtmlSysSoundDigitized_win32()
    {
    }

    /*
     *   Map the CHtmlSysSound interface to the CHtmlSysSoundDigitized_win32
     *   implementation
     */
    int play_sound(class CHtmlSysWin *win,
                   void (*done_func)(void *, int),
                   void *done_func_ctx, int repeat,
                   const textchar_t *url,
                   int vol, long fade_in, long fade_out, int crossfade_out)
    {
        return CHtmlSysSoundDigitized_win32::play_sound(
            win, done_func, done_func_ctx, repeat, url,
            vol, fade_in, fade_out, crossfade_out);
    }

    /* add a crossfade */
    void add_crossfade(class CHtmlSysWin *win, long ms)
        { CHtmlSysSoundDigitized_win32::add_crossfade(win, ms); }

    /* cancel the sound */
    void cancel_sound(
        class CHtmlSysWin *win, int sync, long fade_out_ms, int fade_in_bg)
    {
        CHtmlSysSoundDigitized_win32::cancel_sound(
            win, sync, fade_out_ms, fade_in_bg);
    }

    int maybe_suspend(class CHtmlSysSound *new_sound)
        { return CHtmlSysSoundDigitized_win32::maybe_suspend(new_sound); }
    void resume()
        { CHtmlSysSoundDigitized_win32::resume(); }

protected:
    /* create the player object */
    virtual CTadsCompressedAudio *create_player(
        const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
        IDirectSound *ds, class CTadsAudioControl *ctl,
        HWND hwnd, void (*done_func)(void *, int), void *done_func_ctx);
};

/* ------------------------------------------------------------------------ */
/*
 *   Ogg Vorbis sound implementation
 */
class CHtmlSysSoundOgg_win32: public CHtmlSysSoundOgg,
    public CHtmlSysSoundDigitized_win32
{
public:
    /*
     *   CHtmlSysSoundOgg implementation
     */
    CHtmlSysSoundOgg_win32()
        : CHtmlSysSoundOgg(), CHtmlSysSoundDigitized_win32()
    {
    }

    /*
     *   Map the CHtmlSysSound interface to the CHtmlSysSoundDigitized_win32
     *   implementation
     */
    int play_sound(class CHtmlSysWin *win,
                   void (*done_func)(void *, int),
                   void *done_func_ctx, int repeat,
                   const textchar_t *url,
                   int vol, long fade_in, long fade_out, int crossfade_out)
    {
        return CHtmlSysSoundDigitized_win32::play_sound(
            win, done_func, done_func_ctx, repeat, url,
            vol, fade_in, fade_out, crossfade_out);
    }

    /* add a crossfade */
    void add_crossfade(class CHtmlSysWin *win, long ms)
        { CHtmlSysSoundDigitized_win32::add_crossfade(win, ms); }

    /* cancel the sound */
    void cancel_sound(
        class CHtmlSysWin *win, int sync, long fade_out_ms, int fade_in_bg)
    {
        CHtmlSysSoundDigitized_win32::cancel_sound(
            win, sync, fade_out_ms, fade_in_bg);
    }

    int maybe_suspend(class CHtmlSysSound *new_sound)
        { return CHtmlSysSoundDigitized_win32::maybe_suspend(new_sound); }
    void resume()
        { CHtmlSysSoundDigitized_win32::resume(); }

protected:
    /* create the player object */
    virtual CTadsCompressedAudio *create_player(
        const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
        IDirectSound *ds, class CTadsAudioControl *ctl,
        HWND hwnd, void (*done_func)(void *, int), void *done_func_ctx);
};

#endif /* W32SND_H */


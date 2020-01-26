/* $Header: d:/cvsroot/tads/html/win32/tadssnd.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadssnd.h - TADS sound object
Function

Notes

Modified
  01/10/98 MJRoberts  - Creation
*/

#ifndef TADSSND_H
#define TADSSND_H

#include <math.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif


/*------------------------------------------------------------------------ */
/*
 *   Audio Controller - provides global control over certain aspects of sound
 *   playback.  This is an abstract interface that must be provided to sound
 *   objects when they're created.
 */
class CTadsAudioControl
{
public:
    /* reference control */
    virtual void audioctl_add_ref() = 0;
    virtual void audioctl_release() = 0;

    /* get the current muting status */
    virtual int get_mute_sound() = 0;

    /*
     *   Register/unregister a playing sound.  When the controller needs to
     *   change a global playback parameter globally (such as the muting
     *   status), it'll inform each registered sound object of the change.
     */
    virtual void register_active_sound(class CTadsAudioPlayer *sound) = 0;
    virtual void unregister_active_sound(class CTadsAudioPlayer *sound) = 0;
};

/*
 *   The generic sound player interface.  System sound player objects can
 *   register themselves with the audio controller to receive notifications
 *   of changes to global system sound settings.
 */
class CTadsAudioPlayer
{
public:
    CTadsAudioPlayer(HWND hwnd,
                     void (*done_func)(void *, int), void *done_func_ctx);
    virtual ~CTadsAudioPlayer();

    /* is the track still playing? */
    virtual int is_playing() = 0;

    /* get the starting time of playback, in terms of GetTickCount() time */
    DWORD get_start_time() const { return start_time_; }

    /* terminate playback */
    virtual void stop(int sync) = 0;

    /* have we been terminated? */
    virtual int is_stopped() = 0;

    /*
     *   Get the real-time playback length of the sound, in milliseconds.
     *   Return 0 if it's not possible to determine the length of the track.
     */
    virtual long get_track_len_ms() = 0;

    /* receive notification of a change in muting status */
    virtual void on_mute_change(int mute) = 0;

    /* set the playback volume - 0..10000 (silence to unattenuated) */
    virtual void set_audio_volume(int vol) = 0;

    /*
     *   Immediately call the 'done' callback.  The callback should never be
     *   invoked more than once per play, so if the callback has already been
     *   invoked, this should do nothing.
     */
    virtual void call_done_callback() = 0;

    /* note that we're starting a background fade on this player */
    virtual void note_background_fade() { }

    /* reference counting */
    virtual void AddRef() = 0;
    virtual void Release() = 0;

    /* get our start and stop event handles */
    HANDLE get_start_evt() const { return start_evt_; }
    HANDLE get_stop_evt() const { return stop_evt_; }

    /*
     *   Process the window message we use to handle a 'done' notification.
     *   When the background thread finishes playing a sound, it posts a
     *   message to our associated window; the window in turn calls this
     *   method, which finally calls the callback function provided when the
     *   sound was initiated.  We go through this extra message step, rather
     *   than calling the callback directly from the playback thread, because
     *   we want to ensure that the callback is only ever invoked from the
     *   main thread.
     */
    void on_sound_done(int repeat_count);

    /* send the "sound done" notification to our owner window */
    void send_done_message(int repeat_count);

    /*
     *   Synchronize on stop.  This waits until playback has stopped, the
     *   'done' callback has been invoked, and any background thread has
     *   terminated.
     */
    void sync_on_stop();

protected:
    /*
     *   Playback tracking events: we set started_evt_ when playback starts,
     *   and we set ended_evt_ when playback ends.  These allow other objects
     *   in other threads (e.g., volume control threads) to keep track of
     *   when we start and stop, for better timing coordination.
     */
    HANDLE start_evt_;
    HANDLE stop_evt_;

    /* callback event: we set this when we invoke the 'done' callback */
    HANDLE cb_evt_;

    /* the background thread handle */
    HANDLE thread_hdl_;
    DWORD thread_id_;

    /* starting time, as a GetTickCount() time */
    DWORD start_time_;

    /* owner window */
    HWND hwnd_;

    /* the 'done' callback function */
    void (*done_func_)(void *ctx, int repeat_count);
    void *done_func_ctx_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Volume control.  Each player has an associated volume control, which
 *   mediates muting, fade in, and fade out.
 */
class CTadsAudioVolumeControl
{
public:
    CTadsAudioVolumeControl(CTadsAudioPlayer *player,
                            int init_vol, int fading_in, int muted);
    virtual ~CTadsAudioVolumeControl();

    /* reference counting - for thread safety, use interlocked inc/dec */
    void AddRef()
    {
        InterlockedIncrement(&refcnt_);
    }
    void Release()
    {
        if (InterlockedDecrement(&refcnt_) == 0)
            delete this;
    }

    /*
     *   Set the base playback volume.  This is the volume we use when we're
     *   not muting or fading.  By default, we play back at 100% volume.
     *   This sets the volume on a 0..100 scale, where 0 is silence and 100
     *   is unattenuated.
     */
    void set_base_volume(int level);

    /* mute the sound */
    void mute(int muted);

    /*
     *   Adjust the fade-in or fade-out level.  The actual playback level is
     *   the lesser of the current fade-in and fade-out levels, so that if we
     *   are simultaneously fading in and out, we'll have a nice smooth
     *   triangle envelope that peaks at the intersection of the two fade
     *   curves.  If we have multiple concurrent fades in the same direction,
     *   we'll keep the fade smooth by using the LEADING fade: when fading
     *   in, we'll ignore a new fade-in level that's less than the previous
     *   fade-in level, and when fading out we'll ignore new levels that are
     *   higher than previous levels.  (Of course, muting always takes
     *   precedence over fading.)
     *
     *   The level is given in the range 0..10000, where 0 is silence and
     *   10000 is unattenuated.
     */
    void set_fade_in(int level);
    void set_fade_out(int level);

private:
    /*
     *   Update the playback volume level to match the current parameters -
     *   this computes the volume level based on the current fade and mute
     *   settings, and sets it in the underlying playback object
     */
    void update_level();

    /* the base volume level (0-100) */
    int base_vol_;

    /* are we muted? */
    int muted_;

    /* current fade-in and fade-out levels */
    int fade_in_level_;
    int fade_out_level_;

    /* we are a reference-counted object */
    LONG refcnt_;

    /* critical section object, for synchronization */
    CRITICAL_SECTION critsec_;

    /* the underlying audio player object */
    CTadsAudioPlayer *player_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Fader control
 */
class CTadsAudioFader
{
public:
    CTadsAudioFader(CTadsAudioPlayer *player,
                    CTadsAudioVolumeControl *volctl, long ms);
    virtual ~CTadsAudioFader();

    /*
     *   start the fade - call this after creating the object to initiate the
     *   background thread; after this is called, the fade will run by itself
     */
    void start_fade();

    /* reference counting */
    void AddRef() { InterlockedIncrement(&refcnt_); }
    void Release()
    {
        if (InterlockedDecrement(&refcnt_) == 0)
            delete this;
    }

protected:
    /*
     *   Wait until the appointed start time for the fade action.  Return
     *   true if the fade should proceed, false if not.
     */
    virtual int wait_until_ready();

    /*
     *   Do any final action after the fade is finished.  'ok' is the status
     *   returned from wait_until_ready() - it indicates whether or not we
     *   actually did the fade.
     */
    virtual void after_fade(int ok) { }

    /*
     *   Get the volume at the given point in the fade.  The step is given as
     *   a fraction of the time in the fade, from 0.0 to 1.0.  Returns a
     *   value from 0 (total silence) to 10000 (full volume).
     */
    virtual int get_step_vol(double step) = 0;

    /* set the volume in the volume control */
    virtual void set_volctl_volume(int vol) = 0;

    /* static thread entrypoint */
    static DWORD thread_main(void *ctx)
    {
        /* the thread context is the 'this' object */
        CTadsAudioFader *self = (CTadsAudioFader *)ctx;

        /* run the thread-main member function */
        self->do_thread_main();

        /* release the thread's reference on the fader */
        self->Release();

        /* successful completion */
        return 0;
    }

    /* member function thread handler */
    void do_thread_main();

    /* duration of the fade, in milliseconds */
    long ms_;

    /* reference count */
    LONG refcnt_;

    /* the player associated with the fade */
    CTadsAudioPlayer *player_;

    /* volume control object for the track player */
    CTadsAudioVolumeControl *volctl_;
};

/*
 *   Fade In fader
 */
class CTadsAudioFaderIn: public CTadsAudioFader
{
public:
    CTadsAudioFaderIn(CTadsAudioPlayer *player,
                      CTadsAudioVolumeControl *volctl, long ms)
        : CTadsAudioFader(player, volctl, ms) { }

protected:
    /* when fading in, the volume goes up on each step */
    virtual int get_step_vol(double step)
    {
        /*
         *   Figure the point on the log10 scale from 1 to 10, then normalize
         *   to the 0-10000 range.
         */
        return (int)(log10(1.0 + step*9.0)*10000.0);
    }

    /* set the volume in the volume control */
    virtual void set_volctl_volume(int vol) { volctl_->set_fade_in(vol); }
};

/*
 *   Base Fade Out fader
 */
class CTadsAudioFaderOut: public CTadsAudioFader
{
public:
    CTadsAudioFaderOut(CTadsAudioPlayer *player,
                       CTadsAudioVolumeControl *volctl, long ms)
        : CTadsAudioFader(player, volctl, ms) { }

protected:
    /* when fading down, the volume goes down on each step */
    virtual int get_step_vol(double step)
    {
        /*
         *   Figure the point on the log10 scale from 1 to 10, then normalize
         *   to the 0-10000 range.
         */
        return (int)(log10(10.0 - step*9.0)*10000.0);
    }

    /* after the fade is finished, kill the playback in the player */
    virtual void after_fade(int ok);

    /* set the volume in the volume control */
    virtual void set_volctl_volume(int vol) { volctl_->set_fade_out(vol); }
};

/*
 *   Cancellation Fade Out fader.  This starts a fade-out immediately, then
 *   stops the underlying player once we get to silence.
 */
class CTadsAudioFaderOutCxl: public CTadsAudioFaderOut
{
public:
    CTadsAudioFaderOutCxl(class CTadsAudioPlayer *player,
                          CTadsAudioVolumeControl *volctl, long ms)
        : CTadsAudioFaderOut(player, volctl, ms) { }
};


/*
 *   End-of-track Fade Out fader.  This waits for the duration of the track
 *   minus the fade time, then starts the fade-out.
 */
class CTadsAudioFaderOutEnd: public CTadsAudioFaderOut
{
public:
    CTadsAudioFaderOutEnd(
        CTadsAudioPlayer *player, CTadsAudioVolumeControl *volctl,
        long ms, int crossfade)
        : CTadsAudioFaderOut(player, volctl, ms)
    {
        /* remember the cross-fade status */
        crossfade_ = crossfade;
    }

protected:
    /*
     *   when fading out, we need to wait until 'ms' millseconds before the
     *   end of the track
     */
    virtual int wait_until_ready();

    /* are we doing a cross-fade on the fade-out? */
    int crossfade_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Base windows-specific sound object
 */
class CTadsSound
{
public:
    CTadsSound();
    virtual ~CTadsSound();

    /* load from a sound resource loader */
    int load_from_res(class CHtmlSound *loader);

    /* cancel with fade-out */
    void cancel_sound(class CHtmlSysWin *win, int sync,
                      long fade_out_ms, int fade_in_bg);

    /* add a crossfade */
    void add_crossfade(class CHtmlSysWin *win, long ms);

    /* get/detach the player object */
    virtual class CTadsAudioPlayer *get_player() const = 0;
    virtual void detach_player() = 0;

protected:
    /* file information */
    CStringBuf fname_;
    unsigned long seek_pos_;
    unsigned long data_size_;

    /* volume control */
    CTadsAudioVolumeControl *volctl_;
};


#endif /* TADSSND_H */

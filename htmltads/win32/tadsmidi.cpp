#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsmidi.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsmidi.cpp - TADS MIDI support
Function

Notes

Modified
  01/17/98 MJRoberts  - Creation
*/


#include <stdlib.h>
#include <string.h>

#include <Windows.h>
#include <dmusici.h>
#include <stdio.h>
#include <io.h>

/* include the TADS OS layer for I/O routines */
#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSMIDI_H
#include "tadsmidi.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSISTR_H
#include "tadsistr.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   DirectMusic interface object
 */

/* statics */
IDirectMusicLoader8 *CTadsDirectMusic::loader_ = 0;
int CTadsDirectMusic::inited_ = 0;

/* initialization */
void CTadsDirectMusic::static_init()
{
    /* if we haven't already initialized, do so now */
    if (!inited_)
    {
        /*
         *   Remember that we've attempted this once - even if we fail, don't
         *   attempt it again, since we'd most likely just fail again for the
         *   same reasons (e.g., DirectMusic isn't installed).
         */
        inited_ = TRUE;

        /* presume we won't get a loader */
        loader_ = 0;

        /*
         *   DirectMusic is deprecated, and some components that we need are
         *   not available in Windows Vista and Windows 7 in 64-bit mode.
         *   Don't bother creating the loader if we're on anything 64-bit
         *   from Vista on.
         */

        /* get the OS version... */
        OSVERSIONINFO ver;
        memset(&ver, 0, sizeof(ver));
        ver.dwOSVersionInfoSize = sizeof(ver);
        GetVersionEx(&ver);

        /* ...and the hardware description... */
        SYSTEM_INFO si;
        memset(&si, 0, sizeof(si));
        GetSystemInfo(&si);

        /* ...and also note if we're a 32-bit process on a 64-bit OS */
        BOOL iswow64 = FALSE;
        typedef BOOL (WINAPI *wow64_fn)(HANDLE, PBOOL);
        wow64_fn wow64 = (wow64_fn)GetProcAddress(
            GetModuleHandle("kernel32"), "IsWow64Process");
        if (wow64 != 0)
            wow64(GetCurrentProcess(), &iswow64);

        /*
         *   Okay, we have all our system info: check to see if this is
         *   64-bit Vista (6.0) or later.  DM is presumably not coming back,
         *   so >= 6.0 should be the right test.
         */
        if (ver.dwMajorVersion >= 6
            && (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64
                || si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64
                || iswow64))
            return;

        /* create the music loader */
        if (FAILED(CoCreateInstance(
            CLSID_DirectMusicLoader, 0, CLSCTX_INPROC,
            IID_IDirectMusicLoader8, (void **)&loader_)))
        {
            /* failed to load it - clear it out */
            loader_ = 0;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   MIDI file player implementation
 */

/*
 *   output stream descriptor - this is a private structure that we use to
 *   keep track of our position in an output stream as we fill it
 */
struct midi_stream_info_t
{
    /* starting time for the current block of data in the stream */
    midi_time_t start_time_;

    /* maximum delta time allowed for the stream */
    midi_time_t max_delta_time_;

    /* current write pointer */
    unsigned char *bufptr_;

    /* space remaining in the buffer */
    size_t buflen_;
};

/* number of background fade-outs in progress */
int CTadsMidiFilePlayer::bg_fade_cnt_ = 0;

CTadsMidiFilePlayer::CTadsMidiFilePlayer(
    CTadsAudioControl *ctl, const textchar_t *fname,
    osfildef *fp, long seek_pos, long data_size,
    HWND hwnd, void (*done_func)(void *, int), void *done_func_ctx)
    : CTadsAudioPlayer(hwnd, done_func, done_func_ctx)
{
    /* set a reference on behalf of our caller */
    refcnt_ = 1;

    /* clear out members */
    hmidistrm_ = 0;
    midi_dev_id_ = 0;
    reader_ = 0;
    end_of_stream_ = FALSE;
    num_playing_ = 0;
    err_ = FALSE;
    bg_fading_ = FALSE;
    is_done_ = FALSE;

    /* no DirectMusic objects yet */
    dm_perf_ = 0;
    dm_seg_ = 0;
    dm_buf_ = 0;

    /* create our DirectMusic monitor event */
    dm_monitor_event_ = CreateEvent(0, FALSE, FALSE, 0);

    /* assume full volume initially, unless we hear otherwise */
    cur_vol_ = 10000;

    /* remember and place a reference on the audio controller */
    audio_control_ = ctl;
    ctl->audioctl_add_ref();

    /* remember our source file name */
    fname_.set(fname);

    /* create a private handle, to isolate our use from other threads */
    fp_ = fdopen(_dup(_fileno(fp)), "rb");

    /* remember the starting seek position in the file */
    file_start_pos_ = seek_pos;
    file_size_ = data_size;

    /* note the initial mute status */
    muted_ = ctl->get_mute_sound();

    /* initialize our critical section object */
    InitializeCriticalSection(&critsec_);
}

CTadsMidiFilePlayer::~CTadsMidiFilePlayer()
{
    /* close the MIDI stream  */
    close_stream();

    /* release DirectMusic objects */
    release_dm_objs();

    /* delete the reader */
    if (reader_ != 0)
        delete reader_;

    /* done with our monitor event */
    CloseHandle(dm_monitor_event_);

    /* release our audio controller interface */
    audio_control_->audioctl_release();

    /* delete our critical section object */
    DeleteCriticalSection(&critsec_);

    /* done with our private copy of the source file */
    fclose(fp_);
}

/*
 *   Release the DirectMusic objects
 */
void CTadsMidiFilePlayer::release_dm_objs()
{
    /* release DirectMusic objects */
    if (dm_perf_ != 0)
    {
        dm_perf_->CloseDown();
        dm_perf_->Release();
        dm_perf_ = 0;
    }
    if (dm_seg_ != 0)
    {
        dm_seg_->Release();
        dm_seg_ = 0;
    }
    if (dm_buf_ != 0)
    {
        dm_buf_->Release();
        dm_buf_ = 0;
    }

}

/*
 *   Begin playing a MIDI file.  Returns zero on success, non-zero on
 *   failure.
 */
int CTadsMidiFilePlayer::play()
{
    int err = 0;

    /* haven't yet reached the end */
    end_of_stream_ = FALSE;
    stopping_ = FALSE;
    is_done_ = FALSE;

    /* no errors yet */
    err_ = FALSE;

    /* no buffers playing yet */
    num_playing_ = 0;

    /* if we already have a stream, close it */
    close_stream();

    /* likewise any old DirectMusic objects */
    release_dm_objs();

    /* always start at tick offset zero in the midi stream */
    start_tick_pos_ = 0;

    /*
     *   Try playing back via DirectMusic.  If that fails, fall back on the
     *   Win32 midiXxx() APIs.
     */
    IDirectMusicLoader8 *dm_loader = CTadsDirectMusic::get_loader();
    if (dm_loader != 0
        && (dm_perf_ = CTadsDirectMusic::create_performance()) != 0)
    {
        /* remember the buffer for the audio path in the performance */
        dm_buf_ = CTadsDirectMusic::get_ds_buffer(dm_perf_);

        /* create the file reader stream */
        IStream *str = new CTadsStdioIStreamReader(
            fp_, file_start_pos_, file_size_);

        /* load the file from the stream */
        DMUS_OBJECTDESC desc;
        static int midi_serial = 0;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwValidData = DMUS_OBJ_NAME | DMUS_OBJ_STREAM | DMUS_OBJ_CLASS;
        desc.guidClass = CLSID_DirectMusicSegment;
        swprintf(desc.wszName, L"MIDI%d", midi_serial++);
        desc.pStream = str;
        if (FAILED(dm_loader->GetObject(
            &desc, IID_IDirectMusicSegment8, (void **)&dm_seg_)))
            dm_seg_ = 0;

        /*
         *   we're done with our reference to the stream now - the segment
         *   will take it from here
         */
        str->Release();

        /* mark the segment as a standard MIDI file */
        if (dm_seg_ != 0
            && FAILED(dm_seg_->SetParam(
                GUID_StandardMIDIFile, 0xFFFFFFFF, 0, 0, 0)))
        {
            dm_seg_->Release();
            dm_seg_ = 0;
        }

        /* download the segment's "band" (synthesizer instruments) */
        if (FAILED(dm_seg_->Download(dm_perf_)))
        {
            dm_seg_->Release();
            dm_seg_ = 0;
        }
    }

    /*
     *   If we failed to set up a DirectMusic player, try it the old way,
     *   using the native Win32 midiXxx() APIs.
     */
    if (dm_seg_ == 0)
        err = set_up_player(hwnd_);

    /*
     *   If we encountered an error setting up our player (either DirectMusic
     *   or our native player), signal the "stop" event and return failure.
     */
    if (err != 0)
    {
        SetEvent(stop_evt_);
        return err;
    }

    /* now that we're playing, register with the audio controller */
    audio_control_->register_active_sound(this);

    /* start playback */
    if (dm_seg_ != 0)
    {
        /*
         *   We're using DirectMusic mode
         */

        /* ask for "segment" notifications (start, stop, etc) */
        dm_perf_->AddNotificationType(GUID_NOTIFICATION_SEGMENT);
        dm_perf_->SetNotificationHandle(dm_monitor_event_, 0);

        /* add a self-reference on behalf of the monitor thread */
        AddRef();

        /* start the monitor thread */
        thread_hdl_ = CreateThread(
            0, 0, (LPTHREAD_START_ROUTINE)dm_monitor_main, (void *)this,
            0, &thread_id_);

        /* if that failed, release the self-ref we created for the thread */
        if (thread_hdl_ == 0)
            Release();

        /* set the initial volume (-1 means we leave it as-is) */
        set_audio_volume(-1);

        /* play our segment */
        if (FAILED(dm_perf_->PlaySegment(dm_seg_, 0, 0, 0)))
            err = 10001;
    }
    else
    {
        /*
         *   We're using legacy midiXxx() mode
         */

        /* note the starting time and signal the "start" event */
        start_time_ = GetTickCount();
        SetEvent(start_evt_);

        /*
         *   Prime the stream buffers by sending out the initial chunks.  The
         *   MIDI player will then call us back to refill the buffers each
         *   time they're exhausted, so the process is self-sustaining once
         *   started.
         */
        if (dm_seg_ == 0)
            err = prime_stream();
    }

    /* if an error occurred, signal the "stop" event */
    if (err != 0)
        SetEvent(stop_evt_);

    /* return the status */
    return err;
}

/*
 *   DirectMusic playback thread monitor
 */
void CTadsMidiFilePlayer::dm_monitor()
{
    IDirectMusicPerformance *perf;

    /*
     *   get my own copy of the performance handle, in case the other thread
     *   drops the member copy (which it can do at the end of the playback)
     */
    perf = dm_perf_;
    perf->AddRef();

    /* watch for events */
    for (int done = FALSE ; !done ; )
    {
        /* wait for an event notification */
        WaitForSingleObject(dm_monitor_event_, 100);

        /* process all notifications */
        DMUS_NOTIFICATION_PMSG *pmsg;
        while (perf->GetNotificationPMsg(&pmsg) == S_OK)
        {
            /* check what we have */
            if (pmsg->guidNotificationType == GUID_NOTIFICATION_SEGMENT)
            {
                switch (pmsg->dwNotificationOption)
                {
                case DMUS_NOTIFICATION_SEGSTART:
                    /* playback has started - note the time */
                    start_time_ = GetTickCount();

                    /* signal the start of playback */
                    SetEvent(start_evt_);
                    break;

                case DMUS_NOTIFICATION_SEGABORT:
                case DMUS_NOTIFICATION_SEGEND:
                    /* playback is finished - notify the callback */
                    call_done_callback();

                    /* signal that playback has finished */
                    SetEvent(stop_evt_);

                    /* we can terminate the monitor thread now */
                    done = TRUE;
                    break;
                }
            }

            /* free the message */
            perf->FreePMsg((DMUS_PMSG *)pmsg);
        }
    }

    /* done with our extra performance pointer */
    perf->Release();
}

/*
 *   Set up a player - this sets up the output stream and reads the file
 *   header.
 */
int CTadsMidiFilePlayer::set_up_player(HWND hwnd)
{
    /*
     *   Try each MIDI device, starting with the Windows MIDI mapper.  (The
     *   MIDI mapper is the preferred device because it's actually a software
     *   synthesizer, so it's behavior is somewhat independent of the sound
     *   card hardware.  All other MIDI devices are the physical sound card
     *   MIDI synthesizers; these are not as uniform, so we'll use them only
     *   if we can't access the MIDI mapper.)
     */
    UINT numMidiDevs = midiOutGetNumDevs();
    for (UINT i = (UINT)-1 ; ; ++i)
    {
        /*
         *   get the current device - i == -1 means the MIDI mapper,
         *   otherwise 'i' is the system device number to try (MIDI devices
         *   are numbered from 0 to midiOutGetNumDevs()-1)
         */
        midi_dev_id_ = (i == (UINT)-1 ? MIDI_MAPPER : i);

        /* try opening a stream on this MIDI device */
        MMRESULT res = midiStreamOpen(
            &hmidistrm_, &midi_dev_id_, (DWORD)1,
            (DWORD)hwnd, (DWORD)this, CALLBACK_WINDOW);

        /* if that succeeded, we can stop searching */
        if (res == MMSYSERR_NOERROR)
            break;

        /* check for "already allocated" */
        if (res == MMSYSERR_ALLOCATED)
        {
            /* if we haven't reached the last device yet, keep looking */
            if (i+1 < numMidiDevs)
                continue;

            /*
             *   This was the last device, so we're out of MIDI streams.  If
             *   we have at least one MIDI fade in progress, wait a few
             *   moments and then try again.  This will effectively serialize
             *   two MIDI tracks that the caller asked to cross-fade - since
             *   we don't seem to have enough MIDI players available to
             *   actually do the cross-fade, serializing is the next best
             *   option.  Only do this when there's a fade - for any other
             *   concurrent access attempts we can just fail, since we don't
             *   claim to support concurrent MIDI playback.
             */
            if (bg_fade_cnt_ != 0)
            {
                /* wait for a moment */
                Sleep(50);

                /*
                 *   start over with the search - start at -2 so that we try
                 *   the MIDI mapper again
                 */
                i = (UINT)-2;
                continue;
            }

            /* we're out of streams, and we don't have a fade, so fail */
            return 1;
        }

        /* on any other error, give up and return failure */
        return 1;
    }

    /* delete any existing reader */
    if (reader_ != 0)
        delete reader_;

    /* read the header */
    reader_ = new CTadsMidiFileReader();
    if (reader_->read_header(fp_, file_start_pos_))
        return 2;

    /* set stream properties */
    MIDIPROPTIMEDIV mptd;
    mptd.cbStruct = sizeof(mptd);
    mptd.dwTimeDiv = reader_->get_time_division();
    if (midiStreamProperty(hmidistrm_, (LPBYTE)&mptd,
                           MIDIPROP_SET | MIDIPROP_TIMEDIV)
        != MMSYSERR_NOERROR)
        return 3;

    /* restart the stream */
    if (midiStreamRestart(hmidistrm_) != MMSYSERR_NOERROR)
        return 4;

    /* success */
    return 0;
}

int CTadsMidiFilePlayer::prime_stream()
{
    CTadsMidiFilePlayerBuffer *buf;
    int i;
    int num_started = 0;

    /*
     *   Start the process going.  For each of our two buffers, load up the
     *   buffer with as much data as will fit, and start it playing.
     */
    for (i = 0, buf = buffers_ ; i < 2 && !end_of_stream_ ; ++i, ++buf)
    {
        size_t buf_used;
        unsigned char *buf_ptr = buf->get_buf();
        size_t buf_avail = buf->get_buf_size();
        size_t extra_used = 0;

        /* set the initial volume */
        set_audio_volume(-1);

        /* fill up this buffer */
        switch(reader_->fill_stream(buf_ptr, buf_avail,
                                    &buf_used, MIDI_TIME_INFINITY,
                                    muted_, cur_vol_))
        {
        case MIDI_STATUS_END_STREAM:
            /*
             *   This is the end of the stream - note that we have nothing
             *   more to write, but play back what we loaded into this buffer
             */
            end_of_stream_ = TRUE;
            break;

        case MIDI_STATUS_BUFFER_FULL:
        case MIDI_STATUS_TIME_LIMIT:
            /*
             *   we've filled up this buffer, and we have more left to write;
             *   play back this buffer and keep going
             */
            break;

        default:
            /* error - can't proceed; abort */
            err_ = TRUE;
            break;
        }

        /* count the extra space we used for the volume control prefix */
        buf_used += extra_used;

        /* if there's an error, stop */
        if (err_)
            break;

        /* prepare this buffer header */
        if (buf->prepare_header(hmidistrm_, this))
        {
            err_ = TRUE;
            break;
        }

        /*
         *   Add the current buffer to the playback list in the MIDI stream
         *   driver
         */
        if (buf->play(this, hmidistrm_, buf_used))
        {
            /* note the error internally, and return failure */
            err_ = TRUE;
            break;
        }

        /* count the playing buffer */
        ++num_playing_;
        ++num_started;
    }

    /* check to see if we encountered an error before starting any buffers */
    if (err_ && num_started == 0)
        return 5;

    /*
     *   The first buffer will finish before the second buffer starts
     *   playing, so the first completion callback we'll get will be for
     *   the first buffer.  So, this is the next buffer we will re-fill.
     */
    next_to_refill_ = 0;

    /* success */
    return 0;
}

/*
 *   Is the track playing?
 */
int CTadsMidiFilePlayer::is_playing()
{
    /* presume we're not playing, in case we don't find an underlying player */
    int playing = FALSE;

    /* if we have a DirectMusic performance, check its status */
    if (dm_perf_ != 0)
        playing = (dm_perf_->IsPlaying(dm_seg_, 0) == S_OK);

    /* if we have a stream, it's playing if we haven't called the callback */
    if (hmidistrm_ != 0)
        playing = (done_func_ != 0);

    /* return the result */
    return playing;
}

/*
 *   Stop playback.  The client-done callback will still be invoked in the
 *   usual manner.  This may return before playback has actually
 *   completed, unless 'sync' is set to true, in which case we must wait
 *   until the sound has been cancelled and the callback has been invoked
 *   before we can return.
 */
void CTadsMidiFilePlayer::stop(int sync)
{
    /* set the stop flag so we know not to start another buffer */
    stopping_ = TRUE;

    /* tell the DirectMusic segment or midiXxx() API stream to stop playing */
    if (dm_seg_ != 0)
    {
        /* stop the performance */
        dm_perf_->Stop(0, 0, 0, 0);
    }
    else if (hmidistrm_ != 0)
    {
        /* stop the stream */
        midiStreamStop(hmidistrm_);
    }

    /* close the stream */
    close_stream();

    /* wait for playback to stop */
    if (sync)
        sync_on_stop();
}

/*
 *   Set the muting status
 */
void CTadsMidiFilePlayer::on_mute_change(int mute)
{
    /* note the new status */
    muted_ = mute;

#if 1
    /*
     *   set the audio volume, indicating no change - this will take into
     *   account the new mute status and readjust the underlying device
     *   volume
     */
    set_audio_volume(-1);

#else
    // old way - deprecated
    MMTIME tm;

    /* get the current playback time */
    tm.wType = TIME_TICKS;
    midiStreamPosition(hmidistrm_, &tm, sizeof(tm));

    /*
     *   The current playback time only tells us how many ticks we've been
     *   playing since we last started, so we need to add this to the base
     *   tick position from the last time we started play to get the rewind
     *   position.  This is also the new base position for the next time we
     *   need to rewind and restart.
     */
    start_tick_pos_ += tm.u.ticks;

    /* stop the stream */
    midiStreamStop(hmidistrm_);

    /* rewind the file */
    reader_->rewind_to_time(start_tick_pos_);

    /* restart the stream from the current position */
    midiStreamRestart(hmidistrm_);
#endif
}

/*
 *   Set the playback volume
 */
void CTadsMidiFilePlayer::set_audio_volume(int vol)
{
    /* make sure we don't try to do this while closing the channel, etc */
    EnterCriticalSection(&critsec_);

    /* if the volume is -1, it means no change */
    if (vol == -1)
        vol = cur_vol_;

    /*
     *   remember the new volume level, so that we can adjust explicit volume
     *   control messages in the playback stream accordingly
     */
    cur_vol_ = vol;

    /* apply muting, if applicable */
    if (muted_)
        vol = 0;

    /*
     *   set the volume in the DirectMusic buffer or MIDI stream, depending
     *   on how we're playing
     */
    if (dm_buf_ != 0)
    {
        /*
         *   Set the volume in the buffer.  Use our usual convention of
         *   setting from 0db attenuation to -60db attenuation (see
         *   CTadsCompressedAudio::set_volume()).
         */
        dm_buf_->SetVolume(DSBVOLUME_MIN + ((vol*6000)/10000 + 4000));
    }
    else if (hmidistrm_ != 0)
    {
        /*
         *   Figure the volume in MIDI terms, in the range 0..0x50 (silence
         *   to full volume).  Note that the full MIDI volume range is
         *   0..0x80, but volume level 0x50 is conventionally the "normal"
         *   playback volume - anything above that is considered loud
         *   playback, so we want to use this as our fade endpoint.
         */
        unsigned int volbyte = (unsigned int)
                               (((double)vol / 10000.0) * 0x50) << 16;

        /*
         *   Set the volume on each channel with an immediate MIDI message.
         *   (The message is ordered from LSB to MSB.  0xBn is a "control
         *   change" message on channel n; the next byte is 7 to indicate
         *   control #7, which is the channel volume control; and the next
         *   byte is the volume level, as computed above.)
         */
        for (unsigned int i = 0 ; i < 15 ; ++i)
            midiOutShortMsg((HMIDIOUT)hmidistrm_, 0x000007B0 | i | volbyte);
    }

    /* done with the exclusion */
    LeaveCriticalSection(&critsec_);
}

/*
 *   Close the stream
 */
void CTadsMidiFilePlayer::close_stream()
{
    /*
     *   this routine can be called from fader threads on exit, so protect
     *   against multiple entry
     */
    EnterCriticalSection(&critsec_);

    /* if we have an open stream, close it */
    if (hmidistrm_ != 0)
    {
        int i;
        CTadsMidiFilePlayerBuffer *buf;

        /* reset the midi output device */
        midiOutReset((HMIDIOUT)hmidistrm_);

        /* unprepare all of the headers */
        for (i = 0, buf = buffers_ ; i < 2 ; ++i, ++buf)
            buf->unprepare_header(hmidistrm_);

        /* close the stream */
        midiStreamClose(hmidistrm_);

        /* we're no longer playing, so unregister with the controller */
        audio_control_->unregister_active_sound(this);

        /* forget the stream */
        hmidistrm_ = 0;

        /* if we were doing a background fade, it's done now */
        if (bg_fading_)
            bg_fade_cnt_ -= 1;
    }

    /* done with the critical section */
    LeaveCriticalSection(&critsec_);
}

/*
 *   MIDI callback - this is a member function reached from the static
 *   callback
 */
void CTadsMidiFilePlayer::do_midi_cb(UINT msg)
{
    /* check the message */
    switch(msg)
    {
    case MOM_DONE:
        /*
         *   We're finished playing back a stream.  If we're not at the
         *   end of the input yet, read the next buffer and start it
         *   playing; otherwise, simply note that the buffer is done.
         */

        /* that's one less buffer playing */
        --num_playing_;

        /*
         *   if we're not at the end of the stream, and there hasn't been
         *   an error, and we're not stopping playback, load up the next
         *   buffer and start it playing
         */
        if (!end_of_stream_ && !err_ && !stopping_)
        {
            size_t buf_used;
            CTadsMidiFilePlayerBuffer *buf;

            /* fill up the next buffer */
            buf = &buffers_[next_to_refill_];
            switch(reader_->fill_stream(buf->get_buf(), buf->get_buf_size(),
                                        &buf_used, MIDI_TIME_INFINITY,
                                        muted_, cur_vol_))
            {
            case MIDI_STATUS_END_STREAM:
                /*
                 *   This is the end of the stream - note that we have
                 *   nothing more to write, but play back what we loaded
                 *   into this buffer
                 */
                end_of_stream_ = TRUE;
                break;

            case MIDI_STATUS_BUFFER_FULL:
            case MIDI_STATUS_TIME_LIMIT:
                /*
                 *   we've filled up this buffer, and we have more left to
                 *   write; play back this buffer and keep going
                 */
                break;

            default:
                /* error - can't proceed - note the error and continue */
                err_ = TRUE;
                break;
            }

            /*
             *   If we didn't encounter an error, add the current buffer
             *   to the playback list in the MIDI stream driver
             */
            if (!err_ && buf->play(this, hmidistrm_, buf_used))
            {
                /* note the error internally */
                err_ = TRUE;
            }

            /* if we started a new buffer playing, count it */
            if (!err_)
            {
                /* that's another buffer playing */
                ++num_playing_;

                /* advance to the next buffer to fill */
                ++next_to_refill_;
                if (next_to_refill_ > 1)
                    next_to_refill_ = 0;
            }
        }

        /*
         *   check to see if that's the last buffer - if so, we've
         *   finished playback
         */
        if (num_playing_ == 0)
        {
            /*
             *   Close our stream.  Do this *before* invoking the client
             *   callback, since we could just start playing the same
             *   sound over again.
             */
            close_stream();

            /*
             *   We're done with playback -- if there's a client callback
             *   for notification of the end of playback, invoke it now.
             *   Note that we ignore the caller's repeat count request, so
             *   we'll always indicate that we've played back once.
             */
            call_done_callback();

            /* signal that playback has finished */
            SetEvent(stop_evt_);

        }

        /*
         *   the original call to midiStreamOut() that eventually led to the
         *   MM_MOM_DONE message that we're currently handling placed a
         *   reference on 'this' to keep it around until the message was
         *   processed; well, it's processed now, so we can remove the
         *   reference
         */
        Release();

        /* done */
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   File player buffer implementation
 */

CTadsMidiFilePlayerBuffer::CTadsMidiFilePlayerBuffer()
{
    /* allocate the data buffer */
    buf_ = new unsigned char[midi_file_buf_size];

    /* note yet prepared */
    prepared_ = FALSE;
}

CTadsMidiFilePlayerBuffer::~CTadsMidiFilePlayerBuffer()
{
    /* free the data buffer */
    delete buf_;
}

/*
 *   prepare the MIDI header
 */
int CTadsMidiFilePlayerBuffer::prepare_header(HMIDISTRM hmidistrm,
                                              CTadsMidiFilePlayer *player)
{
    /* point the header to the data buffer */
    midihdr_.lpData = (LPSTR)buf_;
    midihdr_.dwBufferLength = midi_file_buf_size;
    midihdr_.dwBytesRecorded = 0;

    /*
     *   store the file player object in the user data in the header -
     *   this allows the message handler, which receives the MIDIHDR as
     *   its parameter, to locate the CTadsMidiFilePlayer object to
     *   dispatch the event to the player event callback
     */
    midihdr_.dwUser = (DWORD)player;

    /* initialize the rest of the header */
    midihdr_.dwFlags = 0;
    midihdr_.lpNext = 0;
    midihdr_.reserved = 0;
    midihdr_.dwOffset = 0;
    memset(midihdr_.dwReserved, 0, sizeof(midihdr_.dwReserved));

    /* ask the MIDI system to prepare the header */
    if (midiOutPrepareHeader((HMIDIOUT)hmidistrm, &midihdr_, sizeof(midihdr_))
        != MMSYSERR_NOERROR)
        return 1;

    /* note that we're prepared now */
    prepared_ = TRUE;

    /* success */
    return 0;
}

/*
 *   play the buffer
 */
int CTadsMidiFilePlayerBuffer::play(CTadsMidiFilePlayer *player,
                                    HMIDISTRM hmidistrm, DWORD buf_used)
{
    /* set the amount of data we're using in the buffer */
    midihdr_.dwBytesRecorded = buf_used;

    /* write the buffer to the output */
    if (midiStreamOut(hmidistrm, &midihdr_, sizeof(midihdr_))
        != MMSYSERR_NOERROR)
        return 1;

    /*
     *   since we're using Window event callbacks, midiStreamOut will send us
     *   an MM_MOM_DONE message when it's finished, with our player's 'this'
     *   pointer as a parameter; this effectively means that we've created an
     *   additional reference on the player until that message arrives
     */
    player->AddRef();

    /* success */
    return 0;
}

/*
 *   unprepare the MIDI header
 */
void CTadsMidiFilePlayerBuffer::unprepare_header(HMIDISTRM hmidistrm)
{
    /* if we're not prepared, ignore the call */
    if (!prepared_)
        return;

    /* tell MIDI we're done with the header */
    midiOutUnprepareHeader((HMIDIOUT)hmidistrm, &midihdr_, sizeof(midihdr_));
    prepared_ = FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   MIDI File Reader implementation
 */

CTadsMidiFileReader::CTadsMidiFileReader()
{
    /* we don't have any tracks initially */
    track_cnt_ = 0;
    tracks_ = 0;
}

CTadsMidiFileReader::~CTadsMidiFileReader()
{
    /* if we have any tracks, delete them */
    if (tracks_ != 0)
        delete [] tracks_;
}

/*
 *   Read a MIDI file header and set up our track list.  This must be used
 *   before we can start playing sounds from a MIDI file.  Before calling
 *   this, seek to the location of the file header.
 */
int CTadsMidiFileReader::read_header(osfildef *fp, long file_start_pos)
{
    unsigned char hdrbuf[14];
    unsigned int i;
    CTadsMidiTrackReader *track;

    /* start out at time zero */
    cur_time_ = 0;

    /* we don't have an event yet */
    have_event_ = FALSE;

    /* not at end of stream yet */
    end_of_stream_ = FALSE;

    /* no error yet */
    err_ = FALSE;

    /*
     *   The header starts at the start of the file - seek there.  Note that
     *   this might not be byte 0 in the file, since we could be reading a
     *   resource embedded in a larger file, such as a .gam or .t3 file.
     */
    osfseek(fp, file_start_pos, OSFSK_SET);

    /*
     *   read the MThd header: "MThd", followed by big-endian 32-bit
     *   header size, followed by the file header.
     */
    if (osfrb(fp, hdrbuf, sizeof(hdrbuf)))
        return 1;

    /*
     *   make sure the MThd is present, and make sure the header size
     *   matches what we expect
     */
    if (memcmp(hdrbuf, "MThd", 4) != 0 || conv_midi32(&hdrbuf[4]) != 6)
        return 2;

    /* decode the header fields */
    midi_fmt_ = conv_midi16(&hdrbuf[8]);
    track_cnt_ = conv_midi16(&hdrbuf[10]);
    time_div_ = conv_midi16(&hdrbuf[12]);

    /* allocate space for the tracks, now that we know how many there are */
    tracks_ = new CTadsMidiTrackReader[track_cnt_];

    /* read each track header */
    for (i = 0, track = tracks_ ; i < track_cnt_ ; ++i, ++track)
    {
        unsigned long track_len;
        unsigned long track_ofs;

        /* read the track header */
        if (osfrb(fp, hdrbuf, 8))
            return 3;

        /* make sure the header has the "MTrk" signature */
        if (memcmp(hdrbuf, "MTrk", 4) != 0)
            return 4;

        /* get the length of the track */
        track_len = conv_midi32(&hdrbuf[4]);

        /* note the start of the track */
        track_ofs = osfpos(fp);

        /* initialize the track */
        if (track->init(fp, track_ofs, track_len, time_div_))
            return 5;

        /* skip the rest of the track data */
        osfseek(fp, track_ofs + track_len, OSFSK_SET);
    }

    /* success */
    return 0;
}

/*
 *   Rewind to the start of the MIDI file
 */
int CTadsMidiFileReader::rewind()
{
    unsigned int i;
    CTadsMidiTrackReader *track;

    /* rewind each track */
    for (i = 0, track = tracks_ ; i < track_cnt_ ; ++i, ++track)
    {
        /* rewind this track; return an error if the track does */
        if (track->rewind())
            return 1;
    }

    /* we don't have an event buffered now */
    have_event_ = FALSE;

    /* not at end of stream yet */
    end_of_stream_ = FALSE;

    /* no error yet */
    err_ = FALSE;

    /* success */
    return 0;
}

/*
 *   rewind to a specific time
 */
void CTadsMidiFileReader::rewind_to_time(unsigned long tm)
{
    unsigned int i;
    CTadsMidiTrackReader *track;

    /* rewind completely */
    rewind();

    /* set our time to the requested time */
    cur_time_ = tm;

    /* skip ahead on each track to the requested time */
    for (i = 0, track = tracks_ ; i < track_cnt_ ; ++i, ++track)
        track->skip_to_time(tm);
}

/*
 *   Get the track length
 */
long CTadsMidiFilePlayer::get_track_len_ms()
{
    /*
     *   Set up a new reader for a silent scan - we don't want to use the
     *   main reader, since it might be in the process of playing the file,
     *   and reading from it here would mess up its stream pointers.
     */
    CTadsMidiFileReader *reader = new CTadsMidiFileReader();

    /* create a private copy of the file to avoid thread interference */
    osfildef *fp = fdopen(_dup(_fileno(fp_)), "rb");
    if (fp == 0)
        return 0;

    /* read the header */
    int err = reader->read_header(fp, file_start_pos_);

    /* if we failed to read the header, return failuer */
    if (err)
        return 0;

    /* figure the time */
    long tm = (long)reader->get_play_time_ms();

    /* done with our private file copy */
    fclose(fp);

    /* done with the reader */
    delete reader;

    /* return the result */
    return tm;
}

/*
 *   Get the play time in milliseconds
 */
double CTadsMidiFileReader::get_play_time_ms()
{
    unsigned int i;
    CTadsMidiTrackReader *track;
    double tm;

    /* read events until we run out of events or reach an error */
    while (read_next_event() == MIDI_STATUS_OK)
    {
        /* mark the event we just read as consumed */
        have_event_ = FALSE;
    }

    /* return the end time of the longest track */
    for (i = 0, tm = 0.0, track = tracks_ ; i < track_cnt_ ; ++i, ++track)
    {
        /* if this track has the highest time we've seen so far, save it */
        double cur = track->get_track_time_ms();
        if (cur > tm)
            tm = cur;
    }

    /* return the high water mark */
    return tm;
}

/*
 *   Read data from the MIDI file into a Windows MIDI stream.  This reads
 *   the file and converts information in the file into MIDI stream data.
 */
midi_status_t CTadsMidiFileReader::fill_stream(unsigned char *outbuf,
                                               size_t outbuflen,
                                               size_t *outbufused,
                                               midi_time_t max_delta_time,
                                               int muted, int cur_vol)
{
    midi_status_t stat;
    midi_stream_info_t info;
    int done;

    /* if we have an error, we can't proceed */
    if (err_)
        return MIDI_STATUS_ERROR;

    /* set up our stream descriptor structure */
    info.start_time_ = cur_time_;
    info.max_delta_time_ = max_delta_time;
    info.bufptr_ = outbuf;
    info.buflen_ = outbuflen;

    /* keep going until we encounter an error or fill up the buffer */
    for (done = FALSE ; !done ; )
    {
        /* get the next event */
        stat = read_next_event();
        switch(stat)
        {
        case MIDI_STATUS_END_STREAM:
            /* reached end of stream without an error */
            done = TRUE;
            break;

        case MIDI_STATUS_OK:
            /* no problem; keep going */
            break;

        default:
            /* note that we've encountered an error */
            err_ = TRUE;

            /* return the error */
            done = TRUE;
            break;
        }

        /* exit the loop if we're done */
        if (done)
            break;

        /* add this event to the stream buffer */
        stat = add_to_stream(&event_, &info, muted, cur_vol);
        switch(stat)
        {
        case MIDI_STATUS_OK:
            /* success - keep going */
            break;

        case MIDI_STATUS_EVENT_SKIPPED:
            /* this event was skipped - go on to the next */
            break;

        case MIDI_STATUS_BUFFER_FULL:
        case MIDI_STATUS_TIME_LIMIT:
            /*
             *   we've filled up the buffer, either in space or time;
             *   there's no error, so we can keep going on a subsequent
             *   call, but it's time to stop for now
             */
            done = TRUE;
            break;

        default:
            /*
             *   other conditions are errors - we can't proceed now or on
             *   a subsequent call
             */
            err_ = TRUE;
            done = TRUE;
            break;
        }

        /* exit the loop if we're done */
        if (done)
            break;

        /* we've consumed the current event */
        have_event_ = FALSE;
    }

    /* indicate how many bytes we've consumed */
    *outbufused = outbuflen - info.buflen_;

    /* return the status code that made us stop looping */
    return stat;
}

/*
 *   Read an event into our event buffer
 */
midi_status_t CTadsMidiFileReader::read_next_event()
{
    unsigned int i;
    CTadsMidiTrackReader *track;
    CTadsMidiTrackReader *next_track;
    midi_time_t min_time;

    /* if we already have an event, there's nothing more to do now */
    if (have_event_)
        return MIDI_STATUS_OK;

    /* keep going until we get an event or encounter an error */
    for (;;)
    {
        /*
         *   scan each track and find the one with the event nearest to
         *   the current time
         */
        for (i = 0, track = tracks_,
             min_time = MIDI_TIME_INFINITY, next_track = 0 ;
             i < track_cnt_ ; ++i, ++track)
        {
            /*
             *   Check to see if this track is the soonest so far
             *   (ignoring any tracks that have reached their ends)
             */
            if (!track->end_of_track()
                && track->get_next_event_time() < min_time)
            {
                /* this is the best one so far */
                min_time = track->get_next_event_time();
                next_track = track;
            }
        }

        /* if we didn't find anything, the whole stream is at an end */
        if (next_track == 0)
        {
            end_of_stream_ = TRUE;
            return MIDI_STATUS_END_STREAM;
        }

        /* note the tempo before the event */
        double tempo = next_track->get_ms_per_tick();

        /* get the event from the track */
        if (next_track->read_event(&event_))
        {
            /*
             *   couldn't read the event - set our error flag and return
             *   failure
             */
            err_ = TRUE;
            return MIDI_STATUS_ERROR;
        }

        if ((event_.short_data_[0] & 0xF0) == 0xB0
            && event_.short_data_[1] == 0x07)
        {
            int is_vol_ctl = TRUE;
        }

        /*
         *   if we just read a tempo change from track #0, and this is a
         *   format 1 file, it applies to all of the other tracks as well
         */
        double new_tempo;
        if (next_track == &tracks_[0]
            && midi_fmt_ == 1
            && tempo != (new_tempo = next_track->get_ms_per_tick()))
        {
            /* notify all of the other tracks of the tempo change */
            for (unsigned int j = 0 ; j < track_cnt_ ; ++j)
                tracks_[j].set_ms_per_tick(new_tempo);
        }

        /*
         *   If this is an end-of-track event, go back for another event,
         *   since we may have another track with more data.
         */
        if (event_.short_data_[0] == MIDI_META
            && event_.short_data_[1] == MIDI_META_EOT)
            continue;

        /* we now have an event */
        have_event_ = TRUE;

        /* success */
        return MIDI_STATUS_OK;
    }
}

/*
 *   Add an event to a stream buffer
 */
midi_status_t CTadsMidiFileReader::add_to_stream(
    midi_event_t *event, midi_stream_info_t *info, int muted, int cur_vol)
{
    midi_time_t now;
    midi_time_t delta;
    MIDIEVENT *outevt;
    size_t outlen;

    /* if this event would exceed the time limit, we're done */
    if (cur_time_ - info->start_time_ > info->max_delta_time_)
        return MIDI_STATUS_TIME_LIMIT;

    /* note the current time */
    now = cur_time_;

    /* compute the delta time for the track so far */
    delta = event->time_ - cur_time_;

    /* the current time is this event's time */
    cur_time_ = event->time_;

    /* set up a pointer to the event in the output stream */
    outevt = (MIDIEVENT *)info->bufptr_;

    /* presume we won't write anything */
    outlen = 0;

    /* check the event type */
    if (event->short_data_[0] < MIDI_SYSEX)
    {
        /*
         *   it's a channel message - copy it directly
         */

        /* make sure we have room */
        outlen = 3 * sizeof(DWORD);
        if (info->buflen_ < outlen)
            return MIDI_STATUS_BUFFER_FULL;

        /*
         *   if it's a NOTE ON message, and we're muted, change it to a NOTE
         *   OFF message instead, so that we consume the required time but
         *   don't actually play any music
         */
        if (muted && (event->short_data_[0] & MIDI_NOTEON) == MIDI_NOTEON)
            event->short_data_[0] = (event->short_data_[0] & 0x0F)
                                    | MIDI_NOTEOFF;

        /*
         *   If it's a volume control change, combine the requested volume
         *   with the current master volume.  Volume change messages are of
         *   the form 0xBn 0x07 <vol>, where n is the channel number.
         */
        if (cur_vol != 10000
            && (event->short_data_[0] & 0xF0) == 0xB0
            && event->short_data_[1] == 0x07)
        {
            /*
             *   scale the requested volume: our range 0..10000 maps to MIDI
             *   range 0..0x50
             */
            event->short_data_[1] = (unsigned char)
                                    (((double)event->short_data_[1] / 80.0)
                                     * ((double)cur_vol / 10000.0));
        }

        /* write the data, encoding it with a SHORT flag */
        outevt->dwDeltaTime = delta;
        outevt->dwStreamID = 0;
        outevt->dwEvent = ((DWORD)event->short_data_[0])
                          + (((DWORD)event->short_data_[1]) << 8)
                          + (((DWORD)event->short_data_[2]) << 16)
                          + MEVT_F_SHORT;
    }
    else if (event->short_data_[0] == MIDI_SYSEX
             || event->short_data_[0] == MIDI_SYSEXEND)
    {
        /*
         *   System Exclusive event
         */

        /* ignore system exclusive events */
        return MIDI_STATUS_EVENT_SKIPPED;
    }
    else
    {
        /*
         *   Meta events
         */

        /* make sure it's what we were expecting */
        if (event->short_data_[0] != MIDI_META)
            return MIDI_STATUS_ERROR;

        /* check what type of meta event it is */
        switch(event->short_data_[1])
        {
        case MIDI_META_TEMPO:
            /*
             *   tempo-change event
             */

            /* make sure we have our three bytes of parameters */
            if (event->long_data_len_ != 3)
                return MIDI_STATUS_ERROR;

            /* we need space for 3 DWORD's in the output stream */
            outlen = 3 * sizeof(DWORD);
            if (info->buflen_ < outlen)
                return MIDI_STATUS_BUFFER_FULL;

            /* write out the event */
            outevt->dwDeltaTime = delta;
            outevt->dwStreamID = 0;
            outevt->dwEvent = ((DWORD)event->long_data_[2])
                              + (((DWORD)event->long_data_[1]) << 8)
                              + (((DWORD)event->long_data_[0]) << 16)
                              + (((DWORD)MEVT_TEMPO) << 24)
                              + MEVT_F_SHORT;

            /* done */
            break;

        default:
            /* ignore all other meta events */
            return MIDI_STATUS_EVENT_SKIPPED;
        }
    }


    /* advance pointers */
    info->buflen_ -= outlen;
    info->bufptr_ += outlen;

    /* success */
    return MIDI_STATUS_OK;
}


/* ------------------------------------------------------------------------ */
/*
 *   Track Reader implementation
 */

CTadsMidiTrackReader::CTadsMidiTrackReader()
{
    /* no information yet */
    fp_ = 0;
    file_start_pos_ = 0;
    track_len_ = 0;

    /* no error yet */
    err_ = FALSE;
}

CTadsMidiTrackReader::~CTadsMidiTrackReader()
{
}

/*
 *   Initialize the track
 */
int CTadsMidiTrackReader::init(osfildef *fp, unsigned long file_start_ofs,
                               unsigned long track_len, unsigned int time_div)
{
    /* remember our file information */
    fp_ = fp;
    file_start_pos_ = file_start_ofs;
    track_len_ = track_len;
    time_div_ = time_div;

    /* start at the beginning of the file */
    return rewind();
}

/*
 *   Rewind to the beginning of the track
 */
int CTadsMidiTrackReader::rewind()
{
    unsigned long val;

    /* dump our buffer, and go to the beginning of the track */
    buf_rem_ = 0;
    track_ofs_ = 0;

    /* reset error indication */
    err_ = FALSE;

    /* reset running status byte */
    last_status_ = 0;

    /* if the track length is zero, there's nothing more to do */
    if (track_len_ == 0)
        return 0;

    /* read the time of the first event */
    if (read_varlen(&val))
    {
        /* read failed - set error flag and return failure */
        err_ = TRUE;
        return 1;
    }

    /*
     *   figure the initial timing - the default temp is 120 beats/minute
     *.   = 2 beats/second
     *.   = 8 quarter-notes/second
     *.   = 0.125 seconds/quarter-note
     *.   = 125000 microseconds/quarter-note
     */
    figure_tick_time(125000);

    /* set the starting time, in MIDI ticks and in real time */
    next_event_time_ = (midi_time_t)val;
    cur_time_ms_ = (double)val * ms_per_tick_;

    /* success */
    return 0;
}

/*
 *   Figure the tick time, given the tempo in microseconds per quarter note
 */
void CTadsMidiTrackReader::figure_tick_time(long us_per_qnote)
{
    /* figure the tick time, now that we know the time division */
    if ((time_div_ & 0x8000) == 0)
    {
        /*
         *   The time division is the number of ticks per quarter note.  The
         *   real time per quarter note is specified by the tempo, which is
         *   our microseconds-per-quarter-note parameter.  So:
         *
         *.   (ms/qnote) / (ticks/qnote) -> ms/tick
         */
        double ms_per_qnote = (double)us_per_qnote / 1000.0;
        ms_per_tick_ = ms_per_qnote / (double)time_div_;
    }
    else
    {
        /*
         *   The time division is given in SMPTE time.  The low-order byte
         *   gives ticks/frame, and the high-order byte gives frames per
         *   second, as a negative number.
         */
        int ticks_per_frame = time_div_ & 0x0F;
        int frames_per_sec = (char)(((time_div_ >> 4) & 0x0F) | 0xF0);
        int ticks_per_sec = ticks_per_frame * frames_per_sec;
        ms_per_tick_ = 1000.0 / (double)ticks_per_sec;
    }
}

/*
 *   skip ahead to the requested event time
 */
void CTadsMidiTrackReader::skip_to_time(unsigned long tm)
{
    midi_event_t event;

    /* read and discard events until we get to the target time */
    while (!end_of_track() && next_event_time_ < tm)
        read_event(&event);
}

/*
 *   Read the next event
 */
int CTadsMidiTrackReader::read_event(midi_event_t *event)
{
    unsigned char evttype;

    /* if we're at the end of the track, we can't read anything */
    if (end_of_track())
        return 1;

    /* get the first byte of the event - this specifies the event type */
    if (read_byte(&evttype))
        return 2;

    /* clear out the short data area */
    memset(event->short_data_, 0, sizeof(event->short_data_));

    /* determine what type of message we have */
    if ((evttype & 0x80) == 0)
    {
        /*
         *   The high bit is not set, so this is a channel message that
         *   keeps the status byte from the previous message
         */

        /* it's an error if there's no running status */
        if (last_status_ == 0)
        {
            err_ = TRUE;
            return 3;
        }

        /* set the running status and event type data */
        event->short_data_[0] = last_status_;
        event->short_data_[1] = evttype;

        /* get rid of the channel info - get event type */
        evttype = last_status_ & 0xF0;

        /* presume the event length will be 2 bytes */
        event->event_len_ = 2;

        /* if this event has an extra parameter byte, read it */
        if (has_extra_param(evttype))
        {
            /* read the extra byte */
            if (read_byte(&event->short_data_[2]))
                return 4;

            /* note the extra byte in the event length */
            event->event_len_++;
        }
    }
    else if ((evttype & 0xF0) != MIDI_SYSEX)
    {
        /*
         *   The high bit is set, so this message includes a new status
         *   byte, and the message is not a system exclusive message, so
         *   this must be a normal channel message.
         */

        /* store the status byte, and remember it as the new running status */
        event->short_data_[0] = evttype;
        last_status_ = evttype;

        /* get rid of channel part - get event type */
        evttype &= 0xF0;

        /* presume a two-byte event */
        event->event_len_ = 2;

        /* read the second byte */
        if (read_byte(&event->short_data_[1]))
            return 5;

        /* read the third byte if present */
        if (has_extra_param(evttype))
        {
            /* read the extra byte */
            if (read_byte(&event->short_data_[2]))
                return 6;

            /* count it */
            event->event_len_++;
        }
    }
    else if (evttype == MIDI_SYSEX || evttype == MIDI_SYSEXEND)
    {
        /*
         *   This is a system exclusive message.  Following the type byte
         *   is a variable-length value specifying the number of bytes of
         *   data that follows, then comes the message data.
         */

        /* remember the event type */
        event->short_data_[0] = evttype;

        /* get the length of the data block */
        if (read_varlen(&event->long_data_len_))
            return 7;

        /* allocate space in the event structure for the long data */
        if (event->alloc_long_data(event->long_data_len_))
        {
            /* note the error and return failure */
            err_ = TRUE;
            return 8;
        }

        /* read the data */
        if (read_bytes(event->long_data_, event->long_data_len_))
            return 9;
    }
    else if (evttype == MIDI_META)
    {
        /*
         *   Meta event.  The next byte specifies the event class, then
         *   comes a varying-length value specifying the size of the
         *   parameters, and finally comes the bytes of the parameter
         *   block.
         */
        event->short_data_[0] = evttype;

        /* read the class byte */
        if (read_byte(&event->short_data_[1]))
            return 10;

        /* read the length of the long data */
        if (read_varlen(&event->long_data_len_))
            return 11;

        /* if we have any data, read it */
        if (event->long_data_len_ != 0)
        {
            /* make sure there's room for the long data */
            if (event->alloc_long_data(event->long_data_len_))
            {
                /* note the error and return failure */
                err_ = TRUE;
                return 12;
            }

            /* read the long data */
            if (read_bytes(event->long_data_, event->long_data_len_))
                return 13;
        }

        /* if this is a tempo change, calculate the new real-time tempo */
        if (event->short_data_[1] == MIDI_META_TEMPO)
        {
            /*
             *   The tempo event changes the real-time length of a quarter
             *   note.  This event has three bytes of data giving the new
             *   microseconds per quarter note.
             */
            DWORD us_per_qnote = (DWORD)event->long_data_[2]
                                 + (((DWORD)event->long_data_[1]) << 8)
                                 + (((DWORD)event->long_data_[0]) << 16);

            /* figure the new real-time tempo */
            figure_tick_time(us_per_qnote);
        }

        /*
         *   if this is an end-of-track message, note that we're at the
         *   end
         */
        if (event->short_data_[1] == MIDI_META_EOT)
        {
            /*
             *   set our read pointer to the end of the data, so that we
             *   know not to read anything more
             */
            track_ofs_ = track_len_;
        }
    }
    else
    {
        /* invalid message */
        err_ = TRUE;
        return 14;
    }

    /* use the pre-fetched event time */
    event->time_ = next_event_time_;

    /*
     *   If we're not at the end of the track, read the time of the next
     *   event.  Note that the timestamp is a delta to the start of the
     *   next event, so we must add the delta to the last event's time.
     */
    if (!end_of_track())
    {
        unsigned long longval;

        /* read the delta */
        if (read_varlen(&longval))
            return 15;

        /*
         *   add it to the time of the last event to get the time of the
         *   next event
         */
        next_event_time_ += (midi_time_t)longval;

        /* add it to the real time */
        cur_time_ms_ += longval * ms_per_tick_;
    }

    /* success */
    return 0;
}

/*
 *   Read the next byte
 */
int CTadsMidiTrackReader::read_byte(unsigned char *byteptr)
{
    /* if the buffer is empty, refill it */
    if (buf_rem_ == 0)
    {
        size_t readlen;

        /* if there's an error, we can't proceed */
        if (err_)
            return 1;

        /* fill the buffer if possible, or read the rest of the track */
        readlen = get_bytes_remaining();
        if (readlen > sizeof(buf_))
            readlen = sizeof(buf_);

        /* seek to the proper position in the file and read the bytes */
        osfseek(fp_, file_start_pos_ + track_ofs_, OSFSK_SET);
        if (readlen == 0 || osfrb(fp_, buf_, readlen))
        {
            /* read failed - go to eof and note the error */
            track_ofs_ = track_len_;
            err_ = TRUE;

            /* return an error */
            return 2;
        }

        /* the next byte to be read is the first byte in the buffer */
        buf_ofs_ = 0;
        buf_rem_ = readlen;
    }

    /* return the next byte in the buffer */
    *byteptr = buf_[buf_ofs_];

    /* bump our buffer and file offset */
    ++buf_ofs_;
    ++track_ofs_;

    /* decrement remaining buffer length */
    --buf_rem_;

    /* success */
    return 0;
}

/*
 *   Read a given number of bytes from the track
 */
int CTadsMidiTrackReader::read_bytes(unsigned char *buf, unsigned long len)
{
    /* if we don't have enough left in the file, fail */
    if (!bytes_avail(len))
    {
        err_ = TRUE;
        return 1;
    }

    /* first, read out of our buffer */
    if (buf_rem_ != 0)
    {
        unsigned long amt;

        /*
         *   read the lesser of the amount requested and the space left in
         *   the buffer
         */
        amt = len;
        if (buf_rem_ < amt)
            amt = buf_rem_;

        /* copy bytes out of the buffer */
        memcpy(buf, buf_ + buf_ofs_, (size_t)amt);

        /* advance the destination pointer past the copied bytes */
        buf += amt;
        len -= amt;

        /* adjust counters */
        buf_ofs_ += (size_t)amt;
        track_ofs_ += amt;
        buf_rem_ -= (size_t)amt;
    }

    /*
     *   if we couldn't satisfy the entire request from the buffer, read
     *   any remainder directly from the file
     */
    while (len != 0)
    {
        size_t amt;

        /* in deference to 16-bit machines, read only up to 16k at a time */
        amt = 16*1024;
        if (len < amt)
            amt = len;

        /* read the data */
        osfseek(fp_, file_start_pos_ + track_ofs_, OSFSK_SET);
        if (osfrb(fp_, buf, amt))
        {
            err_ = TRUE;
            return 2;
        }

        /* advance the destination pointer by the amount read */
        buf += amt;
        len -= amt;

        /* adjust our file position counter */
        track_ofs_ += amt;
    }

    /* success */
    return 0;
}

/*
 *   Read a variable-length value from the stream
 */
int CTadsMidiTrackReader::read_varlen(unsigned long *valptr)
{
    unsigned long acc;

    /* start off with the accumulator at zero */
    acc = 0;

    /* read bytes as long as we're not at EOF and there's more to read */
    for (;;)
    {
        unsigned char cur;

        /* if at EOF, abort with an error */
        if (end_of_track())
        {
            /* set error flag and return failure */
            err_ = TRUE;
            return 1;
        }

        /* read the next byte */
        if (read_byte(&cur))
            return 2;

        /*
         *   Add it to the accumulator.  Each byte in the data stream
         *   encodes 7 bits of the value, starting with the highest-order
         *   byte, plus an extra bit (the high-order bit) indicating
         *   whether another byte follows.
         */
        acc <<= 7;
        acc += (cur & 0x7f);

        /* if the 'continue' bit is not set, we're done */
        if ((cur & 0x80) == 0)
            break;
    }

    /* return the result */
    *valptr = acc;
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   MIDI event structure
 */

midi_event_t::midi_event_t()
{
    /* no long data yet */
    long_data_ = 0;
    long_data_alo_ = 0;
}

midi_event_t::~midi_event_t()
{
    /* delete any long data we have */
    if (long_data_ != 0)
        os_free_huge(long_data_);
}

/*
 *   Allocate space for a given amount of long data, if we don't have
 *   space for this much already.
 */
int midi_event_t::alloc_long_data(unsigned long siz)
{
    if (long_data_alo_ < siz)
    {
        /* free any existing long data area */
        if (long_data_ != 0)
            os_free_huge(long_data_);

        /* allocate space for the given amount of data */
        long_data_ = (OS_HUGEPTR(unsigned char))os_alloc_huge(siz);
        if (long_data_ == 0)
            return 1;

        /* note the new size of the long data */
        long_data_alo_ = siz;
    }

    /* success */
    return 0;
}


/* =============================================================================
 * mixer.h -- Sample playback for Windows 11 / XAudio2 2.9
 *
 * A small, sample-only audio mixer. Each logical channel owns one XAudio2
 * source voice; XAudio2 does the mixing, pitch shifting and sample-rate
 * conversion, so there is no software mix loop, no audio thread, and nothing
 * to pump from the game loop.
 *
 * Usage:
 *     mixer_init();
 *     int snd = load_sample(NULL, "data\\explode.wav");
 *     sample_start(0, snd, 0);
 *     sample_set_volume(0, 200);
 *     ...
 *     mixer_end();
 *
 * Samples are played at their native rate and bit depth. 8- and 16-bit PCM,
 * mono or stereo, any sample rate. No load-time conversion of any kind.
 *
 * Threading: every function here must be called from one thread (the game
 * thread). XAudio2 does its own mixing on its own thread; none of the state
 * in mixer.c is shared with it.
 *
 * Copyright (C) 2022-2026  Tim Cottrill
 * SPDX-License-Identifier: GPL-3.0-or-later
 * ============================================================================= */

#pragma once

#ifndef MIXER_H
#define MIXER_H

#ifdef __cplusplus
extern "C" {
#endif

#define MIXER_MAX_CHANNELS 20
#define MIXER_MAX_SAMPLES  256

/* Where mixer_init leaves the master. 80% sits about 2.5 dB below unity, so
   the game does not come out noticeably louder than everything else running
   on the desktop. */
#define MIXER_DEFAULT_MASTER_VOLUME 80

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

/* Brings up XAudio2 and a stereo mastering voice. Returns 0 on success, -1 on
   failure. Safe to call twice; the second call is a no-op returning 0. */
int  mixer_init(void);

/* Stops everything, frees all samples, tears XAudio2 down. */
void mixer_end(void);

/* -------------------------------------------------------------------------
 * Sample loading
 *
 * Pass archname = NULL to read a loose file from disk, or a .zip path to pull
 * the entry out of an archive. Returns a sample number >= 0, or -1 on failure.
 * ------------------------------------------------------------------------- */
int  load_sample(const char *archname, const char *filename);

/* Frees a sample. Any channel currently playing it is stopped first. */
void sample_remove(int samplenum);

/* -------------------------------------------------------------------------
 * Playback
 * ------------------------------------------------------------------------- */

/* Starts samplenum on chanid, looping forever if loop is non-zero. Resets the
   channel's volume, pan and frequency to their defaults (255 / 128 / native),
   so set those after starting, not before. */
void sample_start(int chanid, int samplenum, int loop);

/* Stops immediately and discards the queued audio. */
void sample_stop(int chanid);

/* Leaves the loop but lets the current pass play out to its end. */
void sample_end(int chanid);

/* Non-zero while audio is still queued on the channel. */
int  sample_playing(int chanid);

void samples_stop_all(void);

/* -------------------------------------------------------------------------
 * Per-channel controls
 * ------------------------------------------------------------------------- */

void sample_set_volume(int chanid, int vol);   /* 0..255, 255 = full        */
void sample_set_pan   (int chanid, int pan);   /* 0..255, 128 = centre      */
void sample_set_freq  (int chanid, int freq);  /* Hz; native rate = normal  */

int  sample_get_volume(int chanid);            /* returns what you set      */
int  sample_get_pan   (int chanid);
int  sample_get_freq  (int chanid);

/* -------------------------------------------------------------------------
 * Master output
 * ------------------------------------------------------------------------- */

void mixer_set_master_volume(int percent);     /* 0..100 */
int  mixer_get_master_volume(void);            /* 0..100, exactly what was set */

/* Ducks the output to silence and brings it back. This is separate from the
   volume setting, so mixer_set_master_volume() still works while paused and
   takes effect on restore, and mixer_get_master_volume() keeps reporting the
   setting rather than zero. Both are idempotent. */
void pause_audio(void);
void restore_audio(void);

/* -------------------------------------------------------------------------
 * Lookup and conversion helpers
 * ------------------------------------------------------------------------- */

/* Sample name is the filename with directories and extension stripped. */
const char *numToName(int samplenum);          /* NULL if not loaded */
int         nameToNum(const char *name);       /* -1 if not found    */

int mixer_percent_to_byte(int percent);        /* 0..100 -> 0..255 */
int mixer_byte_to_percent(int vol255);         /* 0..255 -> 0..100 */

#ifdef __cplusplus
}
#endif

#endif /* MIXER_H */

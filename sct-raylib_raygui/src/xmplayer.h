#ifndef SCT_XMPLAYER_H
#define SCT_XMPLAYER_H

/* Embedded-module music player backed by libxmp-lite, rendering into a
 * raylib AudioStream. All libxmp calls happen on the audio callback thread;
 * the UI only posts atomic commands, so no locks are needed. */

enum {
    XMPLAYER_STOPPED = 0,
    XMPLAYER_PLAYING = 1,
    XMPLAYER_PAUSED  = 2,
};

/* Load module from memory and create the audio stream. Returns 0 on success. */
int xmplayer_init(const unsigned char *mod, int len);

/* Play (from stopped, or resume from pause). */
void xmplayer_play(void);

/* Pause playback (resumes with xmplayer_play). */
void xmplayer_pause(void);

/* Stop and rewind to the beginning. */
void xmplayer_stop(void);

/* Release resources. Call after CloseAudioDevice(). */
void xmplayer_free(void);

#endif /* SCT_XMPLAYER_H */

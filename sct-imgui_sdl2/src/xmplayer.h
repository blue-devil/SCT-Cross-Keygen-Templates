#ifndef SCT_XMPLAYER_H
#define SCT_XMPLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Embedded-module music player backed by libxmp-lite, rendering into an
 * SDL2 audio-device callback. All libxmp calls happen on the audio thread;
 * the UI only posts atomic commands, so no locks are needed.
 * Requires SDL_Init(SDL_INIT_AUDIO) before xmplayer_init. */

enum {
    XMPLAYER_STOPPED = 0,
    XMPLAYER_PLAYING = 1,
    XMPLAYER_PAUSED  = 2,
};

/* Load module from memory and open the audio device. Returns 0 on success. */
int xmplayer_init(const unsigned char *mod, int len);

/* Play (from stopped, or resume from pause). */
void xmplayer_play(void);

/* Pause playback (resumes with xmplayer_play). */
void xmplayer_pause(void);

/* Stop and rewind to the beginning. */
void xmplayer_stop(void);

/* Close the audio device and release resources. */
void xmplayer_free(void);

#ifdef __cplusplus
}
#endif

#endif /* SCT_XMPLAYER_H */

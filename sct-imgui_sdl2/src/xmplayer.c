#include "xmplayer.h"

#include <stdatomic.h>
#include <string.h>

#include <SDL.h>
#include "xmp.h"

#define RATE 44100

static xmp_context ctx = NULL;
static SDL_AudioDeviceID dev = 0;

static atomic_int state = XMPLAYER_STOPPED;
static atomic_int cmd = 0; /* 0 none, 1 start, 2 resume, 3 pause, 4 stop */

/* Audio-thread entry point: processes the pending command, then renders. */
static void SDLCALL on_audio(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    int s = atomic_load(&state);
    int c = atomic_load(&cmd);

    if (c != 0) {
        if (c == 1) {                       /* start / restart */
            if (s != XMPLAYER_STOPPED) xmp_end_player(ctx);
            if (xmp_start_player(ctx, RATE, 0) == 0) s = XMPLAYER_PLAYING;
            else s = XMPLAYER_STOPPED;
        } else if (c == 2 && s == XMPLAYER_PAUSED) {
            s = XMPLAYER_PLAYING;           /* resume */
        } else if (c == 3 && s == XMPLAYER_PLAYING) {
            s = XMPLAYER_PAUSED;
        } else if (c == 4 && s != XMPLAYER_STOPPED) {
            xmp_end_player(ctx);
            s = XMPLAYER_STOPPED;
        }
        atomic_store(&cmd, 0);
        atomic_store(&state, s);
    }

    /* libxmp renders interleaved stereo s16 at RATE, matching the device
     * spec negotiated in xmplayer_init; loop count is effectively forever
     * (the parameter is a count limit, not a boolean). */
    if (s == XMPLAYER_PLAYING) {
        if (xmp_play_buffer(ctx, stream, len, 0x7fffffff) < 0)
            memset(stream, 0, (size_t)len);
    } else {
        memset(stream, 0, (size_t)len);
    }
}

int xmplayer_init(const unsigned char *mod, int len)
{
    if (dev != 0) return 0;                 /* already initialized */

    ctx = xmp_create_context();
    if (!ctx) return -1;

    if (xmp_load_module_from_memory(ctx, mod, len) < 0) {
        SDL_Log("XM: module load failed");
        xmp_free_context(ctx);
        ctx = NULL;
        return -1;
    }

    struct xmp_module_info mi;
    xmp_get_module_info(ctx, &mi);
    SDL_Log("XM: '%s' (%d chn, %d patterns)", mi.mod->name, mi.mod->chn, mi.mod->pat);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = on_audio;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        SDL_Log("XM: audio device open failed: %s", SDL_GetError());
        xmp_free_context(ctx);
        ctx = NULL;
        return -1;
    }
    if (have.freq != RATE) {
        /* libxmp renders at a fixed rate; resampling is out of scope, so
         * fail loudly rather than playing at the wrong pitch. */
        SDL_Log("XM: device rate %d != %d", have.freq, RATE);
        xmplayer_free();
        return -1;
    }

    SDL_PauseAudioDevice(dev, 0);           /* callback runs, silent until play */
    return 0;
}

void xmplayer_play(void)
{
    int s = atomic_load(&state);
    if (s == XMPLAYER_STOPPED) atomic_store(&cmd, 1);
    else if (s == XMPLAYER_PAUSED) atomic_store(&cmd, 2);
}

void xmplayer_pause(void)
{
    if (atomic_load(&state) == XMPLAYER_PLAYING) atomic_store(&cmd, 3);
}

void xmplayer_stop(void)
{
    if (atomic_load(&state) != XMPLAYER_STOPPED) atomic_store(&cmd, 4);
}

void xmplayer_free(void)
{
    if (dev != 0) {
        SDL_CloseAudioDevice(dev);
        dev = 0;
    }
    if (ctx) {
        xmp_free_context(ctx);
        ctx = NULL;
    }
}

#include "xmplayer.h"

#include <stdatomic.h>
#include <string.h>

#include "raylib.h"
#include "xmp.h"

#define RATE 44100

static xmp_context ctx = NULL;
static AudioStream stream = { 0 };

static atomic_int state = XMPLAYER_STOPPED;
static atomic_int cmd = 0; /* 0 none, 1 start, 2 resume, 3 pause, 4 stop */

/* Audio-thread entry point: processes the pending command, then renders. */
static void on_audio(void *buffer, unsigned int frames)
{
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

    unsigned int bytes = frames * 2 /* stereo */ * 2 /* s16 */;
    if (s == XMPLAYER_PLAYING) {
        if (xmp_play_buffer(ctx, buffer, bytes, 0x7fffffff) < 0)
            memset(buffer, 0, bytes);
    } else {
        memset(buffer, 0, bytes);
    }
}

int xmplayer_init(const unsigned char *mod, int len)
{
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "XM: no audio device, music disabled");
        return -1;
    }

    ctx = xmp_create_context();
    if (!ctx) return -1;

    if (xmp_load_module_from_memory(ctx, mod, len) < 0) {
        TraceLog(LOG_WARNING, "XM: module load failed");
        xmp_free_context(ctx);
        ctx = NULL;
        return -1;
    }

    struct xmp_module_info mi;
    xmp_get_module_info(ctx, &mi);
    TraceLog(LOG_INFO, "XM: '%s' (%d chn, %d patterns)",
             mi.mod->name, mi.mod->chn, mi.mod->pat);

    stream = LoadAudioStream(RATE, 16, 2);
    SetAudioStreamCallback(stream, on_audio);
    PlayAudioStream(stream);
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
    /* The audio device (and stream/callback) is already gone by now. */
    if (ctx) {
        xmp_free_context(ctx);
        ctx = NULL;
    }
}

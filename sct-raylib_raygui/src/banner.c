#include "banner.h"

#include <stdlib.h>
#include <string.h>

/* Parse per-frame delays (ms) out of a GIF byte stream by walking its blocks.
 * raylib's loader returns the pixels but discards delays, so we scan the
 * Graphic Control Extensions ourselves. Returns malloc'd array or NULL. */
static int *gif_parse_delays(const unsigned char *d, long len, int *count)
{
    if (len < 14) return NULL;

    long p = 13;
    if (d[10] & 0x80) p += 3 * (2 << (d[10] & 7));   /* skip global color table */

    int cap = 8, n = 0;
    int *out = malloc(cap * sizeof(int));
    if (!out) return NULL;
    int pending = 100;                                /* default delay */

    while (p < len) {
        unsigned char b = d[p];
        if (b == 0x3B) break;                         /* trailer */
        if (b == 0x21) {                              /* extension */
            int label = d[p + 1];
            p += 2;
            if (label == 0xF9 && d[p] >= 4) {         /* graphic control */
                int dly = d[p + 2] | (d[p + 3] << 8); /* 1/100 s units */
                pending = dly ? dly * 10 : 100;
            }
            while (p < len && d[p] != 0) p += d[p] + 1; /* skip sub-blocks */
            p++;                                      /* block terminator */
        } else if (b == 0x2C) {                       /* image descriptor */
            int flags = d[p + 8];
            p += 9;
            if (flags & 0x80) p += 3 * (2 << (flags & 7)); /* local color table */
            p++;                                      /* LZW min code size */
            while (p < len && d[p] != 0) p += d[p] + 1;   /* image data sub-blocks */
            p++;
            if (n == cap) {
                cap *= 2;
                int *tmp = realloc(out, cap * sizeof(int));
                if (!tmp) { free(out); return NULL; }
                out = tmp;
            }
            out[n++] = pending;
            pending = 100;
        } else {
            break;                                    /* unknown block, stop */
        }
    }

    if (n == 0) { free(out); return NULL; }
    *count = n;
    return out;
}

int banner_init(Banner *b, const unsigned char *gif, int len)
{
    memset(b, 0, sizeof(*b));

    int frames = 0;
    Image anim = LoadImageAnimFromMemory(".gif", gif, len, &frames);
    if (frames <= 0 || anim.data == NULL) {
        TraceLog(LOG_WARNING, "BANNER: GIF decode failed");
        UnloadImage(anim);
        return -1;
    }

    b->frames = malloc((size_t)frames * sizeof(Texture2D));
    if (!b->frames) { UnloadImage(anim); return -1; }
    b->delay_ms = gif_parse_delays(gif, len, &b->count);
    if (!b->delay_ms || b->count != frames) {
        /* parser mismatch: fall back to a fixed 100 ms per frame */
        free(b->delay_ms);
        b->delay_ms = malloc((size_t)frames * sizeof(int));
        if (!b->delay_ms) { UnloadImage(anim); free(b->frames); return -1; }
        for (int i = 0; i < frames; i++) b->delay_ms[i] = 100;
        b->count = frames;
    }

    for (int i = 0; i < frames; i++) {
        Image sub = ImageFromImage(anim, (Rectangle){ 0, (float)(i * anim.height),
                                                      (float)anim.width, (float)anim.height });
        b->frames[i] = LoadTextureFromImage(sub);
        SetTextureFilter(b->frames[i], TEXTURE_FILTER_POINT);
        SetTextureWrap(b->frames[i], TEXTURE_WRAP_CLAMP);
        UnloadImage(sub);
    }
    TraceLog(LOG_INFO, "BANNER: %d frames %dx%d", frames, anim.width, anim.height);
    UnloadImage(anim);

    b->count = frames;
    b->current = 0;
    b->elapsed = 0.0;
    return 0;
}

void banner_update(Banner *b, float dt)
{
    if (b->count < 2) return;
    b->elapsed += dt;
    double frame_time = b->delay_ms[b->current] / 1000.0;
    if (frame_time <= 0.0) frame_time = 0.1;
    if (b->elapsed >= frame_time) {
        b->elapsed -= frame_time;
        b->current = (b->current + 1) % b->count;
    }
}

void banner_draw(const Banner *b, int x, int y)
{
    if (b->count > 0) DrawTexture(b->frames[b->current], x, y, WHITE);
}

void banner_free(Banner *b)
{
    for (int i = 0; i < b->count; i++) UnloadTexture(b->frames[i]);
    free(b->frames);
    free(b->delay_ms);
    b->frames = NULL;
    b->delay_ms = NULL;
    b->count = 0;
}

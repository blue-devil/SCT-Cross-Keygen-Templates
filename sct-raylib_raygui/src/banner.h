#ifndef SCT_BANNER_H
#define SCT_BANNER_H

#include "raylib.h"

/* Animated GIF banner, decoded from an embedded byte array. */
typedef struct Banner {
    Texture2D *frames;   /* one texture per GIF frame */
    int *delay_ms;       /* per-frame delay in milliseconds */
    int count;           /* number of frames (>=1) */
    int current;         /* currently displayed frame */
    double elapsed;      /* seconds accumulated on current frame */
} Banner;

/* Decode `gif`/`len` (embedded GIF89a) into GPU textures. Returns 0 on success. */
int banner_init(Banner *b, const unsigned char *gif, int len);

/* Advance frame timing. Call once per frame with the frame delta time. */
void banner_update(Banner *b, float dt);

/* Draw the current frame at (x, y). */
void banner_draw(const Banner *b, int x, int y);

/* Release all GPU resources. */
void banner_free(Banner *b);

#endif /* SCT_BANNER_H */

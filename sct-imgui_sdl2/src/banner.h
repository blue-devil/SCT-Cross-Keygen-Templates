#ifndef SCT_BANNER_H
#define SCT_BANNER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Animated GIF banner: decoded with stb_image (which returns per-frame
 * delays, unlike raylib's loader), uploaded as one GL texture per frame. */
typedef struct Banner {
    unsigned int *frames;   /* GL texture name per GIF frame */
    int *delay_ms;          /* per-frame delay in milliseconds */
    int count;              /* number of frames (>=1) */
    int current;            /* currently displayed frame */
    double elapsed;         /* seconds accumulated on current frame */
    int width, height;      /* frame size in pixels */
} Banner;

/* Decode `gif`/`len` and upload textures to the current GL context.
 * Returns 0 on success. */
int banner_init(Banner *b, const unsigned char *gif, int len);

/* Advance frame timing. dt = seconds since last call. */
void banner_update(Banner *b, double dt);

/* GL texture name of the current frame (0 when uninitialized). */
unsigned int banner_tex(const Banner *b);

/* Delete all textures. Needs the GL context current. */
void banner_free(Banner *b);

/* Decode an embedded PNG (e.g. a window icon) to RGBA. stb_image is
 * implemented in this translation unit. Returns 0 on success; *rgba must
 * be freed with free(). */
int banner_decode_png(const unsigned char *png, int len,
                      int *w, int *h, unsigned char **rgba);

#ifdef __cplusplus
}
#endif

#endif /* SCT_BANNER_H */

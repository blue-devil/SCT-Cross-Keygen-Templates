#include "banner.h"

#include <stdlib.h>
#include <string.h>

#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE               /* mingw gl.h is GL 1.1 */
#define GL_CLAMP_TO_EDGE 0x812F
#endif
/* stb_image (vendored, public domain): GIF frames + delays and PNG icons */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#define STB_IMAGE_STATIC
#define STBI_ONLY_GIF
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma GCC diagnostic pop

int banner_init(Banner *b, const unsigned char *gif, int len)
{
    memset(b, 0, sizeof(*b));

    int *delays = NULL;
    int frames = 0, comp = 0;
    unsigned char *rgba = stbi_load_gif_from_memory(gif, len, &delays,
                                                    &b->width, &b->height,
                                                    &frames, &comp, 4);
    if (!rgba || frames <= 0 || b->width <= 0 || b->height <= 0) {
        free(delays);
        stbi_image_free(rgba);
        return -1;
    }

    b->frames = malloc((size_t)frames * sizeof(unsigned int));
    b->delay_ms = malloc((size_t)frames * sizeof(int));
    if (!b->frames || !b->delay_ms) {
        free(b->frames);
        free(b->delay_ms);
        stbi_image_free(rgba);
        free(delays);
        return -1;
    }

    size_t frame_bytes = (size_t)b->width * (size_t)b->height * 4;
    glGenTextures(frames, b->frames);
    for (int i = 0; i < frames; i++) {
        glBindTexture(GL_TEXTURE_2D, b->frames[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, b->width, b->height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba + frame_bytes * (size_t)i);
        b->delay_ms[i] = (delays && delays[i] > 0) ? delays[i] : 100;
    }

    stbi_image_free(rgba);
    free(delays);
    b->count = frames;
    b->current = 0;
    b->elapsed = 0.0;
    return 0;
}

void banner_update(Banner *b, double dt)
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

unsigned int banner_tex(const Banner *b)
{
    return b->count > 0 ? b->frames[b->current] : 0;
}

void banner_free(Banner *b)
{
    if (b->count > 0) glDeleteTextures(b->count, b->frames);
    free(b->frames);
    free(b->delay_ms);
    b->frames = NULL;
    b->delay_ms = NULL;
    b->count = 0;
}

int banner_decode_png(const unsigned char *png, int len,
                      int *w, int *h, unsigned char **rgba)
{
    int comp = 0;
    *rgba = stbi_load_from_memory(png, len, w, h, &comp, 4);
    return *rgba ? 0 : -1;
}

#ifndef SFX_H
#define SFX_H

#include "types.h"

#define SFX_MAX_SOUNDS 64

/*
 * sfx.h
 *
 * Sound effect manager
 */

struct sfx_sound
{
    i32 id;

    // OpenAL sound buffer
    ALuint buffer;
};

struct sfx_manager
{
    // OpenAL
    ALCcontext *ctx;
    ALCdevice *dev;

    // List of loaded sound effects
    struct sfx_sound *list;
};

extern struct sfx_manager *g_sfx;

i32 sfx_init(void);
void sfx_deinit(void);
struct sfx_sound *sfx_load(const char *);
void sfx_free(struct sfx_sound *);

#endif

#include "pch.h"
#include "tagap.h"
#include "sfx.h"

#define SFX_DISABLE

struct sfx_manager *g_sfx;

i32
sfx_init(void)
{
#ifndef SFX_DISABLE
    g_sfx = &g_state.sfx;
    memset(g_sfx, 0, sizeof(struct sfx_manager));

    g_sfx->list = calloc(SFX_MAX_SOUNDS, sizeof(struct sfx_sound));
    for (u32 s = 0; s < SFX_MAX_SOUNDS; ++s)
    {
        g_sfx->list[s].id = -1;
    }

    // Get default device
    const char *device_name = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    g_sfx->dev = alcOpenDevice(device_name);
    if (!g_sfx->dev)
    {
        LOG_ERROR("[sfx] (OpenAL) failed to open device %s", device_name);
        return -1;
    }
    LOG_INFO("[sfx] (OpenAL) using sound card '%s'",
        alcGetString(g_sfx->dev, ALC_DEFAULT_DEVICE_SPECIFIER));

    // Create OpenAL context
    g_sfx->ctx = alcCreateContext(g_sfx->dev, NULL);
    if (!alcMakeContextCurrent(g_sfx->ctx))
    {
        LOG_ERROR("[sfx] (OpenAL) failed to create context");
        alcCloseDevice(g_sfx->dev);
        g_sfx->dev = NULL;
        g_sfx->ctx = NULL;
        return -1;
    }

    LOG_DBUG("[sfx] (OpenAL) context created");
#else
    LOG_DBUG("[sfx] SFX support not compiled in");
#endif
    return 0;
}

void
sfx_deinit(void)
{
#ifndef SFX_DISABLE
    for (u32 s = 0; s < SFX_MAX_SOUNDS; ++s)
    {
        if (g_sfx->list[s].id < 0) continue;
        sfx_free(&g_sfx->list[s]);
    }
    free(g_sfx->list);

    // Cleanup OpenAL
    alcMakeContextCurrent(0);
    if (g_sfx->ctx) alcDestroyContext(g_sfx->ctx);
    if (g_sfx->dev) alcCloseDevice(g_sfx->dev);
#endif
}

/* Load sound effect */
struct sfx_sound *
sfx_load(const char *fpath)
{
#ifndef SFX_DISABLE
    LOG_DBUG("[sfx] loading '%s'", fpath);

    for (u32 i = 0; i < SFX_MAX_SOUNDS; ++i)
    {
        // Get free sound slot
        struct sfx_sound *result = &g_sfx->list[i];
        if (result->id >= 0) continue;
        result->id = i;

        return result;
    }
    LOG_ERROR("[sfx] failed to load '%s' (limit exceeded)", fpath);
#endif
    return NULL;
}

void
sfx_free(struct sfx_sound *s)
{
#ifndef SFX_DISABLE
    // ...
#endif
}

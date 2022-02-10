#ifndef TAGAP_SPRITE_H
#define TAGAP_SPRITE_H

#include "types.h"
#include "tagap_anim.h"
#include "renderer.h"

#define SPRITE_NAME_MAX 32

enum sprite_load_method
{
    // Held in memory until end of game
    // (Currently we ignore this and use dynamic for everything)
    SPRITE_STATIC = 0,

    // Loaded on level load and unloaded at level end
    SPRITE_DYNAMIC = 1,
};

enum tagap_spritevar_id
{
    _SPRITEVAR_UNKNOWN = 0,

    SPRITEVAR_ACTIVE,
    SPRITEVAR_AIM,
    SPRITEVAR_ANGLEFACTOR,
    SPRITEVAR_ANIMATE,
    SPRITEVAR_ANIMATE_TO,
    SPRITEVAR_BIAS,
    SPRITEVAR_BOB,
    SPRITEVAR_CHARGE,
    SPRITEVAR_DIM,
    SPRITEVAR_FACE,
    SPRITEVAR_FLICKER,
    SPRITEVAR_HIDE_MOVE,
    SPRITEVAR_INACTIVE,
    SPRITEVAR_INVERTFACE,
    SPRITEVAR_KEEPFRAME,
    SPRITEVAR_LAYER,
    SPRITEVAR_PULSE,
    SPRITEVAR_NOGROUND,
    SPRITEVAR_NOFACE,
    SPRITEVAR_ROLL,
    SPRITEVAR_SCALE,
    _SPRITEVAR_COUNT,
};

static const char *SPRITEVAR_NAMES[] =
{
    [_SPRITEVAR_UNKNOWN]     = "",
    [SPRITEVAR_ACTIVE]      = "ACTIVE",
    [SPRITEVAR_AIM]         = "AIM",
    [SPRITEVAR_ANGLEFACTOR] = "ANGLEFACTOR",
    [SPRITEVAR_ANIMATE]     = "ANIMATE",
    [SPRITEVAR_ANIMATE_TO]  = "ANIMATE_TO",
    [SPRITEVAR_BIAS]        = "BIAS",
    [SPRITEVAR_BOB]         = "BOB",
    [SPRITEVAR_CHARGE]      = "CHARGE",
    [SPRITEVAR_DIM]         = "DIM",
    [SPRITEVAR_FACE]        = "FACE",
    [SPRITEVAR_FLICKER]     = "FLICKER",
    [SPRITEVAR_HIDE_MOVE]   = "HIDE_MOVE",
    [SPRITEVAR_INACTIVE]    = "INACTIVE",
    [SPRITEVAR_INVERTFACE]  = "INVERTFACE",
    [SPRITEVAR_KEEPFRAME]   = "KEEPFRAME",
    [SPRITEVAR_LAYER]       = "LAYER",
    [SPRITEVAR_PULSE]       = "PULSE",
    [SPRITEVAR_NOGROUND]    = "NOGROUND",
    [SPRITEVAR_NOFACE]      = "NOFACE",
    [SPRITEVAR_ROLL]        = "ROLL",
    [SPRITEVAR_SCALE]       = "SCALE",
};

static inline enum tagap_spritevar_id
lookup_tagap_spritevar(const char *v)
{
    for (u32 i = 0; i < _SPRITEVAR_COUNT; ++i)
    {
        if (strcmp(v, SPRITEVAR_NAMES[i]) == 0)
        {
            return i;
        }
    }
    LOG_WARN("[tagap_think] lookup of SPRITEVAR '%s' yields nothing", v);
    return _SPRITEVAR_UNKNOWN;
}

struct sprite_info
{
    // Full bright flag
    bool bright;

    // Animation style
    enum tagap_anim anim;

    // Offset from entity
    vec2s offset;

    // Name of sprite
    char name[SPRITE_NAME_MAX];

    // Sprite variables
    struct tagap_spritevar
    {
        i32 value;
    } vars[_SPRITEVAR_COUNT];
};

struct tagap_sprite
{
    struct tagap_sprite_frame
    {
        // Current index of the texture in the renderer
        i32 tex_index;
    } *frames;
    u32 frame_count;

    struct renderable *r;
};

#endif

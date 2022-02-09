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

enum sprite_var_id
{
    SPRITEVAR_ACTIVE = 0,
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
    struct sprite_var
    {
        u32 value;
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

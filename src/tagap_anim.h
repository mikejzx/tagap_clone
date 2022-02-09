#ifndef TAGAP_ANIM_H
#define TAGAP_ANIM_H

/*
 * tagap_anim.h
 *
 * Sprite animation parameters
 */

enum tagap_anim
{
    // No animation
    ANIM_NONE,

    // Head movement and eye animation
    ANIM_FACE,

    // Constant panning forward/back/up/down
    ANIM_PANFORWARD, ANIM_PANBACK, ANIM_PANUP, ANIM_PANDOWN,

    // Weapon animation with parametric recoil
    ANIM_WEAPON,

    // Defines sprite an akimbo-only weapon
    ANIM_WEAPON2,

    ANIM_COUNT,
};

static const char *ANIM_NAMES[] =
{
    [ANIM_NONE]        = "NONE",
    [ANIM_FACE]        = "ANIM_FACE",
    [ANIM_PANFORWARD]  = "ANIM_PANFORWARD",
    [ANIM_PANBACK]     = "ANIM_PANBACK",
    [ANIM_PANUP]       = "ANIM_PANUP",
    [ANIM_PANDOWN]     = "ANIM_PANDOWN",
    [ANIM_WEAPON]      = "ANIM_WEAPON",
    [ANIM_WEAPON2]     = "ANIM_WEAPON2",
};

static inline enum tagap_anim
lookup_tagap_anim(const char *a)
{
    for (u32 i = 0; i < ANIM_COUNT; ++i)
    {
        if (strcmp(a, ANIM_NAMES[i]) == 0)
        {
            return i;
        }
    }
    //LOG_WARN("[tagap_anim] lookup of animation '%s' yields nothing", a);
    return ANIM_NONE;
}

#endif

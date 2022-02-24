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

    // Attack panning forward/back/up/down
    ANIM_PANATTF, ANIM_PANATTB, ANIM_PANATTU, ANIM_PANATTD,

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
    [ANIM_PANATTF]     = "ANIM_PANATTF",
    [ANIM_PANATTB]     = "ANIM_PANATTB",
    [ANIM_PANATTU]     = "ANIM_PANATTU",
    [ANIM_PANATTD]     = "ANIM_PANATTD",
    [ANIM_WEAPON]      = "ANIM_WEAPON",
    [ANIM_WEAPON2]     = "ANIM_WEAPON2",
};

CREATE_LOOKUP_FUNC(lookup_tagap_anim, ANIM_NAMES, ANIM_COUNT);

#endif

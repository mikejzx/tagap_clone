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
    NONE,

    // Head movement and eye animation
    ANIM_FACE,

    // Constant panning forward/back/up/down
    ANIM_PANFORWARD, ANIM_PANBACK, ANIM_PANUP, ANIM_PANDOWN,

    // Weapon animation with parametric recoil
    ANIM_WEAPON,

    // Defines sprite an akimbo-only weapon
    ANIM_WEAPON2,
};

#endif

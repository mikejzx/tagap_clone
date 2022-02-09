#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"
#include "tagap_entity.h"

/*
 * player.h
 *
 * State of human-controlled players
 */

enum PLAYER_TYPE
{
    // This is the local player
    PLAYER_LOCAL = 0,

    // Player 1's splitscreener brothers
    // (Not implemented)
    PLAYER_LOCAL_2,
    PLAYER_LOCAL_3,
    PLAYER_LOCAL_4,

    // Networked player; not controlled by this computer
    PLAYER_NET,
};

struct player
{
    // Player entity
    struct tagap_entity *e;
};

#endif

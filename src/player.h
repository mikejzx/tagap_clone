#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"

struct tagap_entity;

/*
 * player.h
 *
 * State of human-controlled players (unused currently)
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

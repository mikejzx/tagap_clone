#ifndef TAGAP_ENTITY_THINK_H
#define TAGAP_ENTITY_THINK_H

/*
 * tagap_entity_think.h
 *
 * Entity "THINK" functions.  Handles 'AI routines', i.e. provides controls to
 * an entity.
 */

struct tagap_entity;

enum tagap_entity_think_id
{
    THINK_NONE = 0,
    THINK_AI_AIM,      // Aim at player
    THINK_AI_CONSTANT, // Constant distance to leader
    THINK_AI_FOLLOW,   // Follow player
    THINK_AI_ITEM,     // Is an item
    THINK_AI_MISSILE,  // Weapon projectile
    THINK_AI_USER,     // Player controlled,
    THINK_AI_WANDER,   // Wandering entity
    THINK_AI_ZOMBIE,   // Zombie logic

    // NON-STANDARD IDS
    //THINK_AI_NETUSER,  // Network-player controlled

    THINK_COUNT,
};

static const char *THINK_NAMES[] =
{
    [THINK_NONE]        = "NONE",
    [THINK_AI_AIM]      = "AI_AIM",
    [THINK_AI_CONSTANT] = "AI_CONSTANT",
    [THINK_AI_FOLLOW]   = "AI_FOLLOW",
    [THINK_AI_ITEM]     = "AI_ITEM",
    [THINK_AI_MISSILE]  = "AI_MISSILE",
    [THINK_AI_USER]     = "AI_USER",
    [THINK_AI_WANDER]   = "AI_WANDER",
    [THINK_AI_ZOMBIE]   = "AI_ZOMBIE",

    //[THINK_AI_NETUSER]  = "AI_NETUSER",
};

CREATE_LOOKUP_FUNC(lookup_tagap_think, THINK_NAMES, THINK_COUNT);

enum tagap_entity_think_attack_mode
{
    THINK_ATTACK_NONE = 0,
    THINK_ATTACK_AI_FIRE,
    THINK_ATTACK_AI_BLOW,
    THINK_ATTACK_MELEE,

    THINK_ATTACK_COUNT,
};

static const char *THINK_ATTACK_NAMES[] =
{
    [THINK_ATTACK_NONE]    = "NONE",
    [THINK_ATTACK_AI_FIRE] = "AI_FIRE",
    [THINK_ATTACK_AI_BLOW] = "AI_BLOW",
    [THINK_ATTACK_MELEE]   = "AI_MELEE",
};

CREATE_LOOKUP_FUNC(lookup_tagap_think_attack,
    THINK_ATTACK_NAMES, THINK_ATTACK_COUNT);

struct tagap_entity_think
{
    enum tagap_entity_think_id mode;
    f32 speed_mod;
    enum tagap_entity_think_attack_mode attack;
    f32 attack_delay;
};

void entity_think(struct tagap_entity *e);

#endif

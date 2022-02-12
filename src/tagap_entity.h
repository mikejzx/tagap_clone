#ifndef TAGAP_ENTITY_H
#define TAGAP_ENTITY_H

#include "tagap_sprite.h"

#define ENTITY_NAME_MAX 128
#define ENTITY_MAX_SPRITES 32

/*
 * TODO: create auto-generators for lookup_ functions
 */

// Values that can be applied to entities
enum tagap_entity_stat_id
{
    STAT_UNKNOWN = 0,

    // AI stats
    // ...

    // Misc
    STAT_CHARGE,
    STAT_DAMAGE,
    STAT_TEMPMISSILE,

    // FX stats
    // ...

    // S_ stats
    STAT_S_AKIMBO,
    STAT_S_HEALTH,
    STAT_S_WEAPON,

    ENTITY_STAT_COUNT,
};

static const char *STAT_NAMES[] =
{
    [STAT_UNKNOWN]     = "",
    [STAT_CHARGE]      = "CHARGE",
    [STAT_DAMAGE]      = "DAMAGE",
    [STAT_TEMPMISSILE] = "TEMPMISSILE",
    [STAT_S_AKIMBO]    = "S_AKIMBO",
    [STAT_S_HEALTH]    = "S_HEALTH",
    [STAT_S_WEAPON]    = "S_WEAPON",
};

static inline enum tagap_entity_stat_id
lookup_tagap_stat(const char *s)
{
    for (u32 i = 0; i < ENTITY_STAT_COUNT; ++i)
    {
        if (strcmp(s, STAT_NAMES[i]) == 0)
        {
            return i;
        }
    }
    //LOG_WARN("[tagap_think] lookup of STAT '%s' yields nothing", s);
    return STAT_UNKNOWN;
}

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
    THINK_AI_NETUSER,  // Network-player controlled

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

static inline enum tagap_entity_think_id
lookup_tagap_think(const char *t)
{
    for (u32 i = 0; i < THINK_COUNT; ++i)
    {
        if (strcmp(t, THINK_NAMES[i]) == 0)
        {
            return i;
        }
    }
    LOG_WARN("[tagap_think] lookup of THINK '%s' yields nothing", t);
    return THINK_NONE;
}

enum tagap_entity_movetype_id
{
    MOVETYPE_NONE = 0, // Static entity
    MOVETYPE_FLY,      // Flying/floating movement
    MOVETYPE_WALK,     // Affected by gravity

    MOVETYPE_COUNT,
};

static const char *MOVETYPE_NAMES[] =
{
    [MOVETYPE_NONE] = "NONE",
    [MOVETYPE_FLY]  = "FLY",
    [MOVETYPE_WALK] = "WALK",
};

static inline enum tagap_entity_movetype_id
lookup_tagap_movetype(const char *m)
{
    for (u32 i = 0; i < MOVETYPE_COUNT; ++i)
    {
        if (strcmp(m, MOVETYPE_NAMES[i]) == 0)
        {
            return i;
        }
    }
    LOG_WARN("[tagap_movetype] lookup of MOVETYPE '%s' yields nothing", m);
    return THINK_NONE;
}

enum tagap_entity_offset_id
{
    OFFSET_UNKNOWN = 0,
    OFFSET_CONST_VELOCITY,
    OFFSET_FX_CONSTANT_FLOAT,
    OFFSET_FX_DEATHEFFECT,
    OFFSET_FX_OFFSET,
    OFFSET_MODEL_OFFSET,
    OFFSET_SIZE,
    OFFSET_WEAPON_CASING,
    OFFSET_WEAPON_MISSILE,
    OFFSET_WEAPON_OFFSET,
    OFFSET_WEAPON_ORIGIN,

    ENTITY_OFFSET_COUNT,
};

static const char *OFFSET_NAMES[] =
{
    [OFFSET_UNKNOWN] = "",
    [OFFSET_CONST_VELOCITY] = "CONST_VELOCITY",
    [OFFSET_FX_CONSTANT_FLOAT] = "FX_CONSTANT_FLOAT",
    [OFFSET_FX_DEATHEFFECT] = "FX_DEATHEFFECT",
    [OFFSET_FX_OFFSET] = "FX_OFFSET",
    [OFFSET_MODEL_OFFSET] = "MODEL_OFFSET",
    [OFFSET_SIZE] = "SIZE",
    [OFFSET_WEAPON_CASING] = "WEAPON_CASING",
    [OFFSET_WEAPON_MISSILE] = "WEAPON_MISSILE",
    [OFFSET_WEAPON_OFFSET] = "WEAPON_OFFSET",
    [OFFSET_WEAPON_ORIGIN] = "WEAPON_ORIGIN",
};

static inline enum tagap_entity_offset_id
lookup_tagap_offset(const char *o)
{
    for (u32 i = 0; i < ENTITY_OFFSET_COUNT; ++i)
    {
        if (strcmp(o, OFFSET_NAMES[i]) == 0)
        {
            return i;
        }
    }
    LOG_WARN("[tagap_movetype] lookup of OFFSET '%s' yields nothing", o);
    return OFFSET_UNKNOWN;
}

// Info loaded once globally at start of game
struct tagap_entity_info
{
    char name[ENTITY_NAME_MAX];

    // List of sprite infos this entity uses, and the particular quirks applied
    // to them on this particular entityy
    struct tagap_entity_sprite sprites[ENTITY_MAX_SPRITES];
    u32 sprite_count;

    // Stat info set with STAT
    i32 stats[ENTITY_STAT_COUNT];

    // AI routine info
    struct tagap_entity_think
    {
        enum tagap_entity_think_id mode;
        f32 speed_mod;
        //enum tagap_entity_think_attack_id attack;
        f32 attack_speed;
    } think;

    // Movetype info
    struct tagap_entity_movetype
    {
        enum tagap_entity_movetype_id type;
        f32 speed;
    } move;

    // Collider size
    // It seems like X-value is only useful one here for some reason
    // Set with OFFSET SIZE command (will implement other OFFSETs in future)
    vec2s offsets[ENTITY_OFFSET_COUNT];

    // GUNENTITY for weapon entity
    struct tagap_entity_info *gun_entity;
};

// Copy info from @b to @a
inline void
entity_info_clone(
    struct tagap_entity_info *a,
    struct tagap_entity_info *b,
    bool skip_name)
{
    if (!skip_name) strcpy(a->name, b->name);

    memcpy(a->sprites, b->sprites,
        ENTITY_MAX_SPRITES * sizeof(struct tagap_entity_sprite));
    a->sprite_count = b->sprite_count;
    memcpy(a->stats, b->stats, ENTITY_STAT_COUNT * sizeof(i32));
    memcpy(&a->think, &b->think, sizeof(struct tagap_entity_think));
    memcpy(&a->move, &b->move, sizeof(struct tagap_entity_movetype));
    memcpy(a->offsets, b->offsets, ENTITY_OFFSET_COUNT * sizeof(vec2s));
}

struct tagap_entity
{
    // Pointer to the entity info
    struct tagap_entity_info *info;

    // Entity that 'owns' this one (e.g. missile owned by player who fired it)
    struct tagap_entity *owner;
    bool with_owner;

    // Whether this entity is active or not
    bool active;

    // Position of the entity
    vec2s position;

    // Facing/angle
    union
    {
        bool facing;
        f32 aim_angle;
    };
    bool flipped;

    // Entity sprite instances
    struct renderable *sprites[ENTITY_MAX_SPRITES];

    struct tagap_entity_input
    {
        // Horizontal/vertical input levels
        f32 horiz,
            vert;
        // Smoothed versions used for movement
        f32 horiz_smooth,
            vert_smooth;

        // Whether to fire weapon
        bool fire;
    } inputs;

    // Normalised velocity
    vec2s velo;

    // Used for bobbing sprites
    f32 bobbing_timer, bobbing_timer_last;

    // Jumping stuff for walking entities
    f32 jump_timer;
    bool jump_reset;

    // ANIM_FACE blinking
    u64 next_blink;
    f32 blink_timer;

    // Missile stuff
    f32 timer_tempmissile;
    f32 attack_timer;

    // Pooling
    // Pool owner stuff
    struct pooled_entity
    {
        i32 mark;
        struct tagap_entity *e;
    } pool[128];
    u32 pool_count;
    // Pooled object info
    bool pooled;
};

void entity_spawn(struct tagap_entity *);
void entity_update(struct tagap_entity *);
void entity_free(struct tagap_entity *);

inline i32
entity_get_rot(struct tagap_entity *e)
{
    if (e->info->think.mode == THINK_AI_AIM ||
        e->info->think.mode == THINK_AI_MISSILE)
    {
        return e->aim_angle;
    }

    return 0;
}

#endif

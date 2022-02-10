#ifndef TAGAP_ENTITY_H
#define TAGAP_ENTITY_H

#include "tagap_sprite.h"

#define ENTITY_NAME_MAX 128
#define ENTITY_MAX_SPRITES 32

// Values that can be applied to entities
enum tagap_entity_stat_id
{
    STAT_UNKNOWN = 0,

    // AI stats
    // ...

    // Misc
    STAT_CHARGE,
    STAT_DAMAGE,

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
    [STAT_UNKNOWN] = "",
    [STAT_CHARGE] = "CHARGE",
    [STAT_DAMAGE] = "DAMAGE",
    [STAT_S_AKIMBO] = "S_AKIMBO",
    [STAT_S_HEALTH] = "S_HEALTH",
    [STAT_S_WEAPON] = "S_WEAPON",
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

// Info loaded once globally at start of game
struct tagap_entity_info
{
    char name[ENTITY_NAME_MAX];

    // Sprite info set with SPRITE
    // SPRITEVAR modifies the vars member of this
    struct sprite_info sprite_infos[ENTITY_MAX_SPRITES];
    u32 sprite_info_count;

    // Stat info set with STAT
    i32 stats[ENTITY_STAT_COUNT];

    // AI routine info
    struct tagap_entity_think
    {
        enum tagap_entity_think_id mode;
        f32 speed_mod;
        // ... AI attack reference and delay b/w attacks not implemented
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
    vec2s colsize;
};

// Copy info from @b to @a
inline void
entity_info_clone(
    struct tagap_entity_info *a,
    struct tagap_entity_info *b,
    bool skip_name)
{
    if (!skip_name) strcpy(a->name, b->name);

    memcpy(a->sprite_infos, b->sprite_infos,
        ENTITY_MAX_SPRITES * sizeof(struct sprite_info));
    a->sprite_info_count = b->sprite_info_count;
    memcpy(a->stats, b->stats, ENTITY_STAT_COUNT * sizeof(i32));
    memcpy(&a->think, &b->think, sizeof(struct tagap_entity_think));
    memcpy(&a->move, &b->move, sizeof(struct tagap_entity_movetype));
    a->colsize = b->colsize;
}

struct tagap_entity
{
    // Pointer to the entity info
    struct tagap_entity_info *info;

    // Position of the entity
    vec2s position;

    // Facing/angle
    union
    {
        bool facing;
        f32 aim_angle;
    };
    bool flipped;

    // Entity sprites
    struct tagap_sprite sprites[ENTITY_MAX_SPRITES];

    struct tagap_entity_input
    {
        // Horizontal/vertical input levels
        f32 horiz,
            vert;
        // Smoothed versions used for movement
        f32 horiz_smooth,
            vert_smooth;
    } inputs;

    // Normalised velocity
    vec2s velo;

    // Used for bobbing sprites
    f32 bobbing_timer;

    // Jumping stuff for walking entities
    f32 jump_timer;
    bool jump_reset;

    // ANIM_FACE blinking
    u64 next_blink;
    i32 blinked_frames;
};

void entity_spawn(struct tagap_entity *);
void entity_update(struct tagap_entity *);
void entity_free(struct tagap_entity *);

inline i32
entity_get_rot(struct tagap_entity *e)
{
    if (e->info->think.mode == THINK_AI_AIM)
    {
        return e->aim_angle;
    }

    return 0;
}

#endif

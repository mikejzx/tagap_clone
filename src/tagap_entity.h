#ifndef TAGAP_ENTITY_H
#define TAGAP_ENTITY_H

#include "tagap_sprite.h"
#include "tagap_entity_think.h"
#include "tagap_entity_movetype.h"
#include "tagap_entity_fx.h"
#include "tagap_weapon.h"
#include "collision.h"
#include "entity_pool.h"

#define ENTITY_NAME_MAX 128
#define ENTITY_MAX_SPRITES 32
#define PLAYER_WEAPON_COUNT 10
#define PLAYER_MAX_EFFECTS 10
#define WEAPON_MAX_MULTISHOT 32

// Values that can be applied to entities
enum tagap_entity_stat_id
{
    STAT_UNKNOWN = 0,

    // AI stats
    // ...

    // Misc
    STAT_CHARGE,
    STAT_DAMAGE,
    STAT_MULTISHOT,
    STAT_TEMPMISSILE,

    // FX stats
    STAT_FX_BULLET,
    STAT_FX_DIM,
    STAT_FX_DISABLE,
    STAT_FX_EXPAND,
    STAT_FX_FADE,
    STAT_FX_FLOAT,
    STAT_FX_MUZZLE,
    STAT_FX_OFFSXFACE,
    STAT_FX_PUSHUP,
    STAT_FX_RENDERFIRST,
    STAT_FX_SMOKE,

    // S_ stats
    STAT_S_AKIMBO,
    STAT_S_HEALTH,
    STAT_S_WEAPON,

    ENTITY_STAT_COUNT,
};

static const char *STAT_NAMES[] =
{
    [STAT_UNKNOWN]        = "",
    [STAT_CHARGE]         = "CHARGE",
    [STAT_DAMAGE]         = "DAMAGE",
    [STAT_MULTISHOT]      = "MULTISHOT",
    [STAT_TEMPMISSILE]    = "TEMPMISSILE",
    [STAT_FX_BULLET]      = "FX_BULLET",
    [STAT_FX_DIM]         = "FX_DIM",
    [STAT_FX_DISABLE]     = "FX_DISABLE",
    [STAT_FX_EXPAND]      = "FX_EXPAND",
    [STAT_FX_FADE]        = "FX_FADE",
    [STAT_FX_FLOAT]       = "FX_FLOAT",
    [STAT_FX_MUZZLE]      = "FX_MUZZLE",
    [STAT_FX_OFFSXFACE]   = "FX_OFFSXFACE",
    [STAT_FX_PUSHUP]      = "FX_PUSHUP",
    [STAT_FX_RENDERFIRST] = "FX_RENDERFIRST",
    [STAT_FX_SMOKE]       = "FX_SMOKE",
    [STAT_S_AKIMBO]       = "S_AKIMBO",
    [STAT_S_HEALTH]       = "S_HEALTH",
    [STAT_S_WEAPON]       = "S_WEAPON",
};

CREATE_LOOKUP_FUNC(lookup_tagap_stat, STAT_NAMES, ENTITY_STAT_COUNT);

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

CREATE_LOOKUP_FUNC(lookup_tagap_offset, OFFSET_NAMES, ENTITY_OFFSET_COUNT);

// Info loaded once globally at start of game
struct tagap_entity_info
{
    char name[ENTITY_NAME_MAX];

    // Pool in which this entity belongs
    enum entity_pool_id pool_id;

    // List of sprite infos this entity uses, and the particular quirks applied
    // to them on this particular entityy
    struct tagap_entity_sprite sprites[ENTITY_MAX_SPRITES];
    u32 sprite_count;

    // Stat info set with STAT
    i32 stats[ENTITY_STAT_COUNT];

    // AI routine info
    struct tagap_entity_think think;

    // Movetype info
    struct tagap_entity_movetype move;

    // Collider size
    // It seems like X-value is only useful one here for some reason
    // Set with OFFSET SIZE command (will implement other OFFSETs in future)
    vec2s offsets[ENTITY_OFFSET_COUNT];

    // GUNENTITY for weapon entity
    struct tagap_entity_info *gun_entity;

    // Entity ammo amounts (e.g. amount picked up by entity)
    i32 ammo[WEAPON_SLOT_COUNT];

    // Whether this entity has a weapon
    bool has_weapon;

    // Light attached to the entity
    struct tagap_entity_light
    {
        f32 radius;    // Light radius (%)
        f32 intensity; // Light intensity (%)
        vec3s colour;
    } light;

    // Flashlight attached to entity
    struct tagap_entity_flashlight
    {
        vec2s origin;
        f32 halo_radius;
        f32 beam_length;
        vec3s colour;
    } flashlight;

    // NOTE: make sure to add any new members to entity_info_clone!
};

// Copy info from @b to @a
inline void
entity_info_clone(
    struct tagap_entity_info *a,
    struct tagap_entity_info *b,
    bool skip_name)
{
    // Skip names if needed
    if (!skip_name) strcpy(a->name, b->name);

    // DO NOT COPY:
    // * Pool ID

    // Copy all data
    memcpy(a->sprites, b->sprites,
        ENTITY_MAX_SPRITES * sizeof(struct tagap_entity_sprite));
    a->sprite_count = b->sprite_count;
    memcpy(a->stats, b->stats, ENTITY_STAT_COUNT * sizeof(i32));
    memcpy(&a->think, &b->think, sizeof(struct tagap_entity_think));
    memcpy(&a->move, &b->move, sizeof(struct tagap_entity_movetype));
    memcpy(a->offsets, b->offsets, ENTITY_OFFSET_COUNT * sizeof(vec2s));
    a->gun_entity = b->gun_entity;
    memcpy(a->ammo, b->ammo, sizeof(i32) * WEAPON_SLOT_COUNT);
    a->has_weapon = b->has_weapon;
    memcpy(&a->light, &b->light, sizeof(struct tagap_entity_light));
    memcpy(&a->flashlight, &b->flashlight,
        sizeof(struct tagap_entity_flashlight));
}

struct tagap_entity
{
    // Whether this entity is spawned in yet
    bool is_spawned;

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

        // Whether to fire weapon.
        bool fire;

        // Whether to belly-slide
        bool bellyslide;
    } inputs;

    // Normalised velocity
    vec2s velo;

    // Collision info
    struct collision_result collision;

    // Used for bobbing sprites
    f32 bobbing_timer, bobbing_timer_last;

    // Jumping stuff for walking entities
    f32 jump_timer;
    bool jump_reset;

    // Player belly sliding
    f32 slide_timer;

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

    // Weapon info
    // Slot 0 does not have ammo (in the game it defines the clip size)
    struct
    {
        u16 ammo;
        struct tagap_entity *gunent;
        f32 reload_timer;
        bool has_akimbo;
    } weapons[WEAPON_SLOT_COUNT];
    i32 weapon_slot;
    f32 weapon_kick_timer;
    bool firing_now;
    u32 weapon_multishot;
    f32 weapon_multishot_angles[WEAPON_MAX_MULTISHOT];

    // FX data
    struct tagap_entity_fx fx;
};

void entity_spawn(struct tagap_entity *);
void entity_update(struct tagap_entity *);
void entity_free(struct tagap_entity *);
void entity_die(struct tagap_entity *);
void entity_reset(struct tagap_entity *, vec2s, f32, bool);
void entity_set_inactive_hidden(struct tagap_entity *, bool);
void entity_change_weapon_slot(struct tagap_entity *, i32);

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
